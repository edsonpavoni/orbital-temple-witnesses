// Witness foundation · SerialCLI
// Reusable line-based serial command registry. Reads lines from Serial,
// trims, splits into command + args, dispatches to a registered handler.
// Built-in 'help' and '?' enumerate registered commands.

#pragma once

#include <stdint.h>
#include <stddef.h>

class SerialCLI {
 public:
  // Plain C function pointer. Handler receives the args portion (everything
  // after the first whitespace), or "" if no args.
  using Handler = void(*)(const char* args);

  void begin(uint32_t baud = 115200);
  // name and help_text must point to literals or otherwise stable storage.
  void registerCommand(const char* name, Handler handler, const char* help_text);
  // Pump from main loop().
  void loop();
  // Prints "> ".
  void printPrompt();

 private:
  void handleLine_(char* line);
  void splitFirstToken_(char* line, char*& cmd_out, char*& args_out);
  void printHelp_();

  // Buffer matches prior monolith (96 chars). Long enough for "pospid p i d".
  static constexpr size_t kLineCap = 96;
  static constexpr size_t kMaxCmds = 24;

  struct Entry {
    const char* name;
    Handler     handler;
    const char* help_text;
  };

  Entry  cmds_[kMaxCmds];
  size_t cmd_count_ = 0;

  char   line_buf_[kLineCap];
  size_t line_len_ = 0;
};
