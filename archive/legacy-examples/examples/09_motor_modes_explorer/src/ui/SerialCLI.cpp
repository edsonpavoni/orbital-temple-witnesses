// Witness foundation · SerialCLI implementation

#include "SerialCLI.h"

#include <Arduino.h>
#include <string.h>

void SerialCLI::begin(uint32_t baud) {
  Serial.begin(baud);
  // Make Serial.print non-blocking when host disconnects.
  Serial.setTxTimeoutMs(0);
}

void SerialCLI::registerCommand(const char* name, Handler handler, const char* help_text) {
  if (cmd_count_ >= kMaxCmds) return;  // silently drop overflow; caller bug
  cmds_[cmd_count_++] = Entry{ name, handler, help_text };
}

void SerialCLI::printPrompt() {
  Serial.print("> ");
}

void SerialCLI::printHelp_() {
  Serial.println("Commands:");
  for (size_t i = 0; i < cmd_count_; i++) {
    const Entry& e = cmds_[i];
    // Two-column-ish: pad name to 12 chars so help_text aligns.
    Serial.printf("  %-12s %s\n", e.name, e.help_text ? e.help_text : "");
  }
  Serial.println("  help | ?     this help");
}

void SerialCLI::splitFirstToken_(char* line, char*& cmd_out, char*& args_out) {
  // Skip leading whitespace.
  while (*line == ' ' || *line == '\t') line++;
  cmd_out = line;
  args_out = line;
  if (!*line) { args_out = line; return; }
  // Walk to first whitespace - terminates the command token.
  while (*line && *line != ' ' && *line != '\t') line++;
  if (*line) {
    *line++ = '\0';
    while (*line == ' ' || *line == '\t') line++;
  }
  args_out = line;
}

void SerialCLI::handleLine_(char* line) {
  // Skip wholly-empty lines (just Enter).
  char* trim = line;
  while (*trim == ' ' || *trim == '\t') trim++;
  if (*trim == '\0') { printPrompt(); return; }

  char* cmd  = nullptr;
  char* args = nullptr;
  splitFirstToken_(line, cmd, args);
  if (!cmd || !*cmd) { printPrompt(); return; }

  // Built-ins first so user can't accidentally override.
  if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) {
    printHelp_();
    printPrompt();
    return;
  }

  for (size_t i = 0; i < cmd_count_; i++) {
    if (!strcmp(cmd, cmds_[i].name)) {
      cmds_[i].handler(args ? args : "");
      printPrompt();
      return;
    }
  }

  Serial.printf("unknown: %s — try 'help'\n", cmd);
  printPrompt();
}

void SerialCLI::loop() {
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) break;
    if (c == '\n' || c == '\r') {
      if (line_len_ == 0) continue;        // ignore bare CR after LF or vice versa
      line_buf_[line_len_] = '\0';
      handleLine_(line_buf_);
      line_len_ = 0;
    } else if (line_len_ < kLineCap - 1) {
      line_buf_[line_len_++] = (char)c;
    }
    // Overflow chars dropped silently - exploration sketch, not safety-critical.
  }
}
