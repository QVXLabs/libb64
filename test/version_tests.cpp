/* The version macros are generated from the top-level VERSION file. These
   checks are bump-stable (no hard-coded numbers): they confirm the generated
   header is present and internally consistent, and that the legacy per-header
   aliases still track the canonical BASE64_VERSION_* macros. */

#include <gtest/gtest.h>

#include <string>

#include <b64/version.h>
#include <b64/ccommon.h>
#include <b64/cencode.h>
#include <b64/cdecode.h>

TEST(Version, StringMatchesComponents)
{
	const std::string expected = std::to_string(BASE64_VERSION_MAJOR) + "." +
	                             std::to_string(BASE64_VERSION_MINOR) + "." +
	                             std::to_string(BASE64_VERSION_PATCH);
	EXPECT_EQ(std::string(BASE64_VERSION_STRING), expected);
}

TEST(Version, NumericIsOrdered)
{
	EXPECT_EQ(BASE64_VERSION, BASE64_VERSION_MAJOR * 10000 +
	                          BASE64_VERSION_MINOR * 100 +
	                          BASE64_VERSION_PATCH);
}

TEST(Version, LegacyAliasesTrackCanonical)
{
	EXPECT_EQ(BASE64_VER_MAJOR, BASE64_VERSION_MAJOR);
	EXPECT_EQ(BASE64_VER_MINOR, BASE64_VERSION_MINOR);
	EXPECT_EQ(BASE64_CENC_VER_MAJOR, BASE64_VERSION_MAJOR);
	EXPECT_EQ(BASE64_CENC_VER_MINOR, BASE64_VERSION_MINOR);
	EXPECT_EQ(BASE64_CDEC_VER_MAJOR, BASE64_VERSION_MAJOR);
	EXPECT_EQ(BASE64_CDEC_VER_MINOR, BASE64_VERSION_MINOR);
}
