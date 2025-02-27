// Copyright 2011, University of Freiburg, Chair of Algorithms and Data
// Structures.
// Author: Björn Buchhold (buchhold@informatik.uni-freiburg.de)

#pragma once

#include <unicode/bytestream.h>
#include <unicode/casemap.h>

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

using std::string;
using std::string_view;

namespace ad_utility {
//! Utility functions for string. Can possibly be changed to
//! a templated version using std::basic_string<T> instead of
//! std::string. However, it is not required so far.

//! Returns the longest prefix that the two arguments have in common
inline string_view commonPrefix(string_view a, string_view b);

//! Case transformations. Should be thread safe.
inline string getLowercase(const string& orig);

inline string getUppercase(const string& orig);

inline string getLowercaseUtf8(std::string_view s);

inline std::pair<size_t, std::string> getUTF8Prefix(std::string_view s,
                                                    size_t prefixLength);

//! Gets the last part of a string that is somehow split by the given separator.
inline string getLastPartOfString(const string& text, const char separator);

/**
 * @brief Return the last position where <literalEnd> was found in the <input>
 * without being escaped by backslashes. If it is not found at all, string::npos
 * is returned.
 */
inline size_t findLiteralEnd(std::string_view input,
                             std::string_view literalEnd);

// *****************************************************************************
// Definitions:
// *****************************************************************************

// ____________________________________________________________________________
string_view commonPrefix(string_view a, const string_view b) {
  size_t maxIdx = std::min(a.size(), b.size());
  size_t i = 0;
  while (i < maxIdx) {
    if (a[i] != b[i]) {
      break;
    }
    ++i;
  }
  return a.substr(0, i);
}

// ____________________________________________________________________________
string getLowercase(const string& orig) {
  string retVal;
  retVal.reserve(orig.size());
  for (size_t i = 0; i < orig.size(); ++i) {
    retVal += tolower(orig[i]);
  }
  return retVal;
}

// ____________________________________________________________________________
string getUppercase(const string& orig) {
  string retVal;
  retVal.reserve(orig.size());
  for (size_t i = 0; i < orig.size(); ++i) {
    retVal += toupper(orig[i]);
  }
  return retVal;
}

// ____________________________________________________________________________
/*
 * @brief convert a UTF-8 String to lowercase according to the held locale
 * @param s UTF-8 encoded string
 * @return The lowercase version of s, also encoded as UTF-8
 */
std::string getLowercaseUtf8(std::string_view s) {
  std::string result;
  icu::StringByteSink<std::string> sink(&result);
  UErrorCode err = U_ZERO_ERROR;
  icu::CaseMap::utf8ToLower(
      "", 0, icu::StringPiece{s.data(), static_cast<int32_t>(s.size())}, sink,
      nullptr, err);
  if (U_FAILURE(err)) {
    throw std::runtime_error(u_errorName(err));
  }
  return result;
}

/**
 * @brief get a prefix of a utf-8 string of a specified length
 *
 * Returns first min(prefixLength, numCodepointsInInput) codepoints in the UTF-8
 string sv.

 * CAVEAT: The result is often misleading when looking for an answer to the
 * question "is X a prefix of Y" because collation might ignore aspects like
 * punctuation or case.
 * This is currently only used for the text index where all words that
 * share a common prefix of a certain length are stored in the same block.
 * @param sv a UTF-8 encoded string
 * @param prefixLength The number of Unicode codepoints we want to extract.
 * @return the first max(prefixLength, numCodepointsInArgSP) Unicode
 * codepoints of sv, encoded as UTF-8
 */
std::pair<size_t, std::string> getUTF8Prefix(std::string_view sv,
                                             size_t prefixLength) {
  const char* s = sv.data();
  int32_t length = sv.length();
  size_t numCodepoints = 0;
  int32_t i = 0;
  for (i = 0; i < length && numCodepoints < prefixLength;) {
    UChar32 c;
    U8_NEXT(s, i, length, c);
    if (c >= 0) {
      ++numCodepoints;
    } else {
      throw std::runtime_error(
          "Illegal UTF sequence in ad_utility::getUTF8Prefix");
    }
  }
  return {numCodepoints, std::string(sv.data(), i)};
}

// ____________________________________________________________________________
string getLastPartOfString(const string& text, const char separator) {
  size_t pos = text.rfind(separator);
  if (pos != text.npos) {
    return text.substr(pos + 1);
  } else {
    return text;
  }
}

// _________________________________________________________________________
inline size_t findLiteralEnd(const std::string_view input,
                             const std::string_view literalEnd) {
  // keep track of the last position where the literalEnd was found unescaped
  auto lastFoundPos = size_t(-1);
  auto endPos = input.find(literalEnd, 0);
  while (endPos != string::npos) {
    if (endPos > 0 && input[endPos - 1] == '\\') {
      size_t numBackslash = 1;
      auto slashPos = endPos - 2;
      // the first condition checks > 0 for unsigned numbers
      while (slashPos < input.size() && input[slashPos] == '\\') {
        slashPos--;
        numBackslash++;
      }
      if (numBackslash % 2 == 0) {
        // even number of backslashes means that the quote we found has not
        // been escaped
        break;
      }
      endPos = input.find(literalEnd, endPos + 1);
    } else {
      // no backslash before the literalEnd, mark this as a candidate position
      lastFoundPos = endPos;
      endPos = input.find(literalEnd, endPos + 1);
    }
  }

  // if we have found any unescaped occurence of literalEnd, return the last
  // of these positions
  if (lastFoundPos != size_t(-1)) {
    return lastFoundPos;
  }
  return endPos;
}

}  // namespace ad_utility

// these overloads are missing in the STL
// they can be constexpr once the compiler completely supports C++20
inline std::string operator+(const std::string& a, std::string_view b) {
  std::string res;
  res.reserve(a.size() + b.size());
  res += a;
  res += b;
  return res;
}

inline std::string operator+(char c, std::string_view b) {
  std::string res;
  res.reserve(1 + b.size());
  res += c;
  res += b;
  return res;
}
