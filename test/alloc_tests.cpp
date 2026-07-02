/* Tests for the optional customer allocator (b64/alloc.h): the one-shot
   allocating C APIs and the C++ stream wrappers routing their scratch buffers
   through a caller-supplied realloc-style callback with a lifetime hint. */

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

#include "b64_test_util.h"

#include <b64/alloc.h>
#include <b64/encode.h>
#include <b64/decode.h>

namespace {

// Tracking allocator: counts outstanding blocks and tags by lifetime, and can
// be flipped to fail every allocation.
struct TrackingCtx
{
	int live = 0;
	int short_allocs = 0;
	int long_allocs = 0;
	int frees = 0;
	bool fail = false;
	int fail_after = -1;  // fail once this many allocations succeeded; -1 never
};

void* tracking_realloc(void* ctx, void* ptr, size_t size, b64_memlife life)
{
	TrackingCtx* c = static_cast<TrackingCtx*>(ctx);
	if (size == 0)
	{
		if (ptr) { c->frees++; c->live--; }
		std::free(ptr);
		return 0;
	}
	if (c->fail || c->fail_after == 0)
		return 0;
	if (c->fail_after > 0)
		c->fail_after--;
	void* p = std::realloc(ptr, size);
	if (p && !ptr)
	{
		c->live++;
		if (life == B64_MEM_SHORT) c->short_allocs++; else c->long_allocs++;
	}
	return p;
}

} // namespace

// One-shot APIs with the default (NULL) allocator round-trip across sizes, and
// the encoded buffer is NUL-terminated at the reported length.
TEST(Alloc, DefaultRoundTrip)
{
	for (size_t n : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(4),
	                 size_t(100), size_t(1000), size_t(4096)})
	{
		const std::string in = b64test::pattern(n);
		char* enc = 0; size_t elen = 0;
		ASSERT_EQ(base64_encode_alloc(0, in.data(), in.size(), &enc, &elen), 0)
			<< "n=" << n;
		ASSERT_NE(enc, (char*)0);
		EXPECT_EQ(std::strlen(enc), elen);            // NUL-terminated
		EXPECT_EQ(enc, b64test::encode(in));          // matches the block API

		void* dec = 0; size_t dlen = 0;
		ASSERT_EQ(base64_decode_alloc(0, enc, elen, &dec, &dlen), 0);
		EXPECT_EQ(std::string(static_cast<char*>(dec), dlen), in) << "n=" << n;

		base64_free(0, enc);
		base64_free(0, dec);
	}
}

// The custom allocator and its context are used; output buffers are tagged
// long-term, and base64_free balances every allocation.
TEST(Alloc, CustomAllocatorLongLifetime)
{
	TrackingCtx ctx;
	b64_allocator a = { tracking_realloc, &ctx };
	const std::string in = b64test::pattern(1000);

	char* enc = 0; size_t elen = 0;
	ASSERT_EQ(base64_encode_alloc(&a, in.data(), in.size(), &enc, &elen), 0);
	EXPECT_EQ(ctx.long_allocs, 1);
	EXPECT_EQ(ctx.short_allocs, 0);

	void* dec = 0; size_t dlen = 0;
	ASSERT_EQ(base64_decode_alloc(&a, enc, elen, &dec, &dlen), 0);
	EXPECT_EQ(ctx.long_allocs, 2);
	EXPECT_EQ(std::string(static_cast<char*>(dec), dlen), in);

	base64_free(&a, enc);
	base64_free(&a, dec);
	EXPECT_EQ(ctx.live, 0);
	EXPECT_EQ(ctx.frees, 2);
}

// Allocation failure is reported, with out-params cleared.
TEST(Alloc, FailureReturnsError)
{
	TrackingCtx ctx; ctx.fail = true;
	b64_allocator a = { tracking_realloc, &ctx };

	char* enc = reinterpret_cast<char*>(0x1); size_t elen = 99;
	EXPECT_NE(base64_encode_alloc(&a, "abc", 3, &enc, &elen), 0);
	EXPECT_EQ(enc, (char*)0);
	EXPECT_EQ(elen, 0u);

	void* dec = reinterpret_cast<void*>(0x1); size_t dlen = 99;
	EXPECT_NE(base64_decode_alloc(&a, "YWJj", 4, &dec, &dlen), 0);
	EXPECT_EQ(dec, (void*)0);
	EXPECT_EQ(dlen, 0u);
}

// The C base64_allocator() helper builds a usable allocator from fn + ctx.
TEST(Alloc, CHelperBuildsAllocator)
{
	TrackingCtx ctx;
	b64_allocator a = base64_allocator(tracking_realloc, &ctx);

	char* enc = 0; size_t elen = 0;
	ASSERT_EQ(base64_encode_alloc(&a, "abc", 3, &enc, &elen), 0);
	EXPECT_EQ(ctx.long_allocs, 1);
	EXPECT_EQ(std::string(enc, elen), "YWJj");

	base64_free(&a, enc);
	EXPECT_EQ(ctx.live, 0);
}

// The C++ stream wrappers route their scratch buffers through the allocator
// with the short-term hint, free them all, and still round-trip.
TEST(Alloc, CppWrappersUseShortLifetime)
{
	const std::string in = b64test::pattern(5000);

	TrackingCtx ectx;
	std::istringstream is(in);
	std::ostringstream os;
	{
		base64::encoder E = base64::encoder_builder()
			.realloc(tracking_realloc, &ectx).build();
		E.encode(is, os);
	}
	EXPECT_GT(ectx.short_allocs, 0);
	EXPECT_EQ(ectx.long_allocs, 0);
	EXPECT_EQ(ectx.live, 0);          // wrapper frees its own scratch

	TrackingCtx dctx;
	std::istringstream is2(os.str());
	std::ostringstream os2;
	{
		base64::decoder D = base64::decoder_builder()
			.realloc(tracking_realloc, &dctx).build();
		D.decode(is2, os2);
	}
	EXPECT_GT(dctx.short_allocs, 0);
	EXPECT_EQ(dctx.live, 0);
	EXPECT_EQ(os2.str(), in);
}

// A scratch allocation failure in the streaming wrappers surfaces as
// bad_alloc (it used to be a null-pointer write) and doesn't leak the
// buffer that was already allocated.
TEST(Alloc, CppWrappersScratchFailureThrows)
{
	for (int fail_after : {0, 1})
	{
		TrackingCtx ectx; ectx.fail_after = fail_after;
		base64::encoder E = base64::encoder_builder()
			.realloc(tracking_realloc, &ectx).build();
		std::istringstream is("abc");
		std::ostringstream os;
		EXPECT_THROW(E.encode(is, os), std::bad_alloc);
		EXPECT_EQ(ectx.live, 0) << "fail_after=" << fail_after;

		TrackingCtx dctx; dctx.fail_after = fail_after;
		base64::decoder D = base64::decoder_builder()
			.realloc(tracking_realloc, &dctx).build();
		std::istringstream is2("YWJj");
		std::ostringstream os2;
		EXPECT_THROW(D.decode(is2, os2), std::bad_alloc);
		EXPECT_EQ(dctx.live, 0) << "fail_after=" << fail_after;
	}
}
