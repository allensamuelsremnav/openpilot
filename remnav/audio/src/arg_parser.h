#ifndef INCLUDED_arg_parser_h
#define INCLUDED_arg_parser_h

//
// C style diy argument processing.
// Windows does not have getopt and using boost seems
// a overkill.

#include <vector>
#include <stdexcept>
#include <string>
#include <algorithm>

namespace audio {

class ArgParser {
 public:
  ArgParser(int argc, const char* argv[], void (*func)(const char*)=NULL) {
    this->usage = func;
    for (int i = 1; i < argc; i++) {
      this->tokens.push_back(std::string(argv[i]));
    }
  }

  std::string getStringOption(const char* option, bool required=0) {
    std::vector<std::string>::const_iterator iter;
    static std::string dummy = "";

    iter = std::find(tokens.begin(), tokens.end(), option);
    if (iter != tokens.end() && ++iter != tokens.end()) {
      return *iter;
    }
    else if (required) {
      std::string msg = "Missing required value for option '" + std::string(option) + "'";
      if (this->usage) usage(msg.c_str());
      throw std::invalid_argument("Require option " + std::string(option));
    }
    
    return dummy;
  }

  bool isBoolOption(const char* option) {
    return std::find(tokens.begin(), tokens.end(), option) != tokens.end();
  }

  int getIntOption(const char* option, bool required=0) {
    char* end_ptr;
    std::string value_s = getStringOption(option, required);
    int result = (int)strtol(value_s.c_str(), &end_ptr, 0);
    return result;
  }

 private:
  std::vector <std::string> tokens;
  void (*usage)(const char* msg);
};

}
#endif
