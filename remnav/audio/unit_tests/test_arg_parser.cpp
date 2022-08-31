#include "arg_parser.h"
#include <iostream>

#include <gtest/gtest.h>

TEST(ArgParserTest, null_options) {
	int argc = 1;
	const char* argv[] = { "ProgramName" };
	audio::ArgParser parser = audio::ArgParser(argc, argv);
	EXPECT_FALSE(parser.isBoolOption("-t"));
	EXPECT_EQ(parser.getStringOption("-f"), "");
}

TEST(ArgParserTest, fname_options) {
	int argc = 3;
	const char* argv[] = { "ProgramName", "-f", "filename"};
	audio::ArgParser parser = audio::ArgParser(argc, argv);
	EXPECT_FALSE(parser.isBoolOption("-t"));
	EXPECT_EQ(parser.getStringOption("-f"), "filename");
}

TEST(ArgParserTest, port_options) {
	int argc = 5;
	const char* argv[] = { "ProgramName", "-f", "filename", "-p", "1234"};
	audio::ArgParser parser = audio::ArgParser(argc, argv);
	EXPECT_FALSE(parser.isBoolOption("-t"));
	EXPECT_EQ(parser.getIntOption("-p"), 1234);
}

TEST(ArgParserTest, hex_port_options) {
	int argc = 5;
	const char* argv[] = { "ProgramName", "-f", "filename", "-p", "0x1234" };
	audio::ArgParser parser = audio::ArgParser(argc, argv);
	EXPECT_FALSE(parser.isBoolOption("-t"));
	EXPECT_EQ(parser.getIntOption("-p"), 0x1234);
}

TEST(ArgParserTest, int_default_value) {
	int argc = 5;
	const char* argv[] = { "ProgramName", "-f", "filename", "-p", "0x1234" };
	audio::ArgParser parser = audio::ArgParser(argc, argv);
	EXPECT_EQ(parser.getIntOption("-b", 0), 0);   // Default value of optional int
}