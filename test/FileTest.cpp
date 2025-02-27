// Copyright 2011, University of Freiburg, Chair of Algorithms and Data
// Structures.
// Author: Björn Buchhold <buchholb>

#include <gtest/gtest.h>
#include <stdio.h>

#include "../src/util/File.h"

using std::string;
using std::vector;

namespace ad_utility {
class FileTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    {
      File testFile1("_tmp_testFile1", "w");
      string content = "line1\nline2\n";
      testFile1.write(content.c_str(), content.size());
    }
    {
      File testFile2("_tmp_testFile2", "w");
      string content = "line1\nline2";
      testFile2.write(content.c_str(), content.size());
    }
    {
      File testFile3("_tmp_testFile3", "w");
      string content = "line1\n";
      testFile3.write(content.c_str(), content.size());
    }
    {
      File testFile3("_tmp_testFile4", "w");
      string content = "";
      testFile3.write(content.c_str(), content.size());
    }
    {
      File testFileBinary("_tmp_testFileBinary", "w");
      size_t a = 1;
      size_t b = 0;
      size_t c = 5000;
      off_t off = 3;
      testFileBinary.write(&a, sizeof(size_t));
      testFileBinary.write(&b, sizeof(size_t));
      testFileBinary.write(&c, sizeof(size_t));
      testFileBinary.write(&off, sizeof(off_t));
    }
  }

  virtual void TearDown() {
    remove("_tmp_testFile1");
    remove("_tmp_testFile2");
    remove("_tmp_testFile3");
    remove("_tmp_testFile4");
    remove("_tmp_testFileBinary");
  }
};

TEST_F(FileTest, testReadLineWithTrailingNewline) {
  File withTrailingNewline("_tmp_testFile1", "r");
  string line;
  vector<string> lines;
  char buf[1024];
  while (withTrailingNewline.readLine(&line, buf, 1024)) {
    lines.push_back(line);
  }
  ASSERT_EQ(static_cast<size_t>(2), lines.size());
  ASSERT_EQ("line1", lines[0]);
  ASSERT_EQ("line2", lines[1]);
}

TEST_F(FileTest, testReadLineWithoutTrailingNewline) {
  File withoutTrailingNewline("_tmp_testFile2", "r");
  string line;
  vector<string> lines;
  char buf[1024];
  // Read the first line, everything works fine.
  while (withoutTrailingNewline.readLine(&line, buf, 1024)) {
    lines.push_back(line);
  }
  ASSERT_EQ(static_cast<size_t>(2), lines.size());
  ASSERT_EQ("line1", lines[0]);
  ASSERT_EQ("line2", lines[1]);
}

TEST_F(FileTest, testReadLineFromEmptyFile) {
  File withoutTrailingNewline("_tmp_testFile4", "r");
  string line;
  vector<string> lines;
  char buf[1024];
  // Read the first line, everything works fine.
  while (withoutTrailingNewline.readLine(&line, buf, 1024)) {
    lines.push_back(line);
  }
  ASSERT_EQ(static_cast<size_t>(0), lines.size());
}

TEST_F(FileTest, testWriteLineAppend) {
  {
    File file3("_tmp_testFile3", "a");
    file3.writeLine("line2");
  }

  File file3("_tmp_testFile3", "r");
  string line;
  vector<string> lines;
  char buf[1024];
  while (file3.readLine(&line, buf, 1024)) {
    lines.push_back(line);
  }
  ASSERT_EQ(static_cast<size_t>(2), lines.size());
  ASSERT_EQ("line1", lines[0]);
  ASSERT_EQ("line2", lines[1]);
}

TEST_F(FileTest, testWriteLineWrite) {
  {
    File file3("_tmp_testFile3", "w");
    file3.writeLine("line2");
  }

  File file3("_tmp_testFile3", "r");
  string line;
  vector<string> lines;
  char buf[1024];
  while (file3.readLine(&line, buf, 1024)) {
    lines.push_back(line);
  }
  ASSERT_EQ(static_cast<size_t>(1), lines.size());
  ASSERT_EQ("line2", lines[0]);
}

TEST_F(FileTest, testReadIntoVector) {
  File withTrailingNewline("_tmp_testFile1", "r");
  File withoutTrailingNewline("_tmp_testFile2", "r");

  vector<string> lines1;
  vector<string> lines2;

  char buf[1024];

  withTrailingNewline.readIntoVector(&lines1, buf, 1024);
  ASSERT_EQ(static_cast<size_t>(2), lines1.size());
  ASSERT_EQ("line1", lines1[0]);
  ASSERT_EQ("line2", lines1[1]);

  withoutTrailingNewline.readIntoVector(&lines2, buf, 1024);
  ASSERT_EQ(static_cast<size_t>(2), lines2.size());
  ASSERT_EQ("line1", lines2[0]);
  ASSERT_EQ("line2", lines2[1]);
}

TEST(File, move) {
  std::string filename = "testFileMove.tmp";
  File file1(filename, "w");
  ASSERT_TRUE(file1.isOpen());
  file1.write("aaa", 3);

  File file2;
  ASSERT_TRUE(file1.isOpen());
  ASSERT_FALSE(file2.isOpen());
  file2 = std::move(file1);
  ASSERT_FALSE(file1.isOpen());
  ASSERT_TRUE(file2.isOpen());

  file2.write("bbb", 3);
  File file3(std::move(file2));
  ASSERT_FALSE(file2.isOpen());
  ASSERT_TRUE(file3.isOpen());
  file3.write("ccc", 3);
  file3.close();

  File fileRead(filename, "r");
  ASSERT_TRUE(fileRead.isOpen());
  std::string s;
  s.resize(2);
  auto numBytes = fileRead.read(s.data(), 2);
  ASSERT_EQ(numBytes, 2u);
  ASSERT_EQ(s, "aa");

  File fileRead2;
  fileRead2 = std::move(fileRead);
  s.resize(5);
  numBytes = fileRead2.read(s.data(), 5);
  ASSERT_EQ(numBytes, 5u);
  ASSERT_EQ(s, "abbbc");

  File fileRead3{std::move(fileRead2)};
  s.resize(2);
  numBytes = fileRead3.read(s.data(), 2);
  ASSERT_EQ(numBytes, 2u);
  ASSERT_EQ(s, "cc");

  ASSERT_EQ(0u, fileRead3.read(s.data(), 9));
  ad_utility::deleteFile(filename);
}
}  // namespace ad_utility
