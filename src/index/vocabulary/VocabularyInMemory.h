// Copyright 2022, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Johannes Kalmbach<joka921> (johannes.kalmbach@gmail.com)

#pragma once

#include <string>
#include <string_view>

#include "../../global/Pattern.h"
#include "../../util/Exception.h"
#include "../CompressedString.h"
#include "../StringSortComparator.h"
#include "./VocabularyTypes.h"

//! A vocabulary. Wraps a `CompactVectorOfStrings<char>`
//! and provides additional methods for reading and writing to/from file,
//! and retrieval via binary search.
class VocabularyInMemory {
 public:
  using CharType = char;
  using StringView = std::basic_string_view<CharType>;
  using String = std::basic_string<CharType>;
  using Words = CompactVectorOfStrings<CharType>;

 private:
  // The actual storage.
  Words _words;

 public:
  /// Construct an empty vocabulary
  VocabularyInMemory() = default;

  /// Construct the vocabulary from `Words`
  explicit VocabularyInMemory(Words words) : _words{std::move(words)} {}

  // Vocabularies are movable
  VocabularyInMemory& operator=(VocabularyInMemory&&) noexcept = default;
  VocabularyInMemory(VocabularyInMemory&&) noexcept = default;

  /// Read the vocabulary from a file. The file must have been created by a call
  /// to `writeToFile` or using a `WordWriter`.
  void open(const string& fileName);

  /// Write the vocabulary to a file.
  void writeToFile(const string& fileName) const;

  /// Return the total number of words
  [[nodiscard]] size_t size() const { return _words.size(); }

  /// Return the highest ID (= index) that occurs in this vocabulary. May only
  /// becalled if size() > 0.
  [[nodiscard]] uint64_t getHighestId() const {
    AD_CHECK(size() > 0);
    return size() - 1;
  }

  /// Return the `i-th` word. The behavior is undefined if `i >= size()`
  auto operator[](uint64_t i) const { return _words[i]; }

  /// Return a `WordAndIndex` that points to the first entry that is equal or
  /// greater than `word` wrt. to the `comparator`. Only works correctly if the
  /// `_words` are sorted according to the comparator (exactly like in
  /// `std::lower_bound`, which is used internally).
  template <typename InternalStringType, typename Comparator>
  WordAndIndex lower_bound(const InternalStringType& word,
                           Comparator comparator) const {
    WordAndIndex result;
    result._index =
        std::lower_bound(_words.begin(), _words.end(), word, comparator) -
        _words.begin();
    result._word = result._index < _words.size()
                       ? std::optional{_words[result._index]}
                       : std::nullopt;
    return result;
  }

  /// Return a `WordAndIndex` that points to the first entry that is greater
  /// than `word` wrt. to the `comparator`. Only works correctly if the `_words`
  /// are sorted according to the comparator (exactly like in
  /// `std::upper_bound`, which is used internally).
  template <typename InternalStringType, typename Comparator>
  WordAndIndex upper_bound(const InternalStringType& word,
                           Comparator comparator) const {
    WordAndIndex result;
    result._index =
        std::upper_bound(_words.begin(), _words.end(), word, comparator) -
        _words.begin();
    result._word = result._index < _words.size()
                       ? std::optional{_words[result._index]}
                       : std::nullopt;
    return result;
  }

  /// A helper type that can be used to directly write a vocabulary to disk
  /// word-by-word, without having to materialize it in RAM first. See the
  /// documentation of `CompactVectorOfStrings` for details.
  using WordWriter = typename Words::Writer;

  /// Create an iterable generator that yields the `VocabularyInMemory` from the
  /// file, without materializing the whole vocabulary in RAM. See the
  /// documentaion of `CompactVectorOfStrings` for details.
  static auto makeWordDiskIterator(const string& filename) {
    return Words::diskIterator(filename);
  }

  /// Clear the vocabulary.
  void close() { _words.clear(); }

  /// Initialize the vocabulary from the given `words`.
  void build(const std::vector<std::string>& words) { _words.build(words); }
};
