#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace brawlers::settings {

using Id = uint64_t;
enum class RowKind { Page, Toggle, Range, Choice, Action, Reset };
struct Value {
  int32_t value = 0;
  std::string label;
};
struct ChoiceUpdate {
  Id row = 0;
  std::vector<Value> values;
};
struct Row {
  Id id = 0;
  RowKind kind = RowKind::Action;
  std::string label;
  std::string description;
  Id page = 0;
  std::vector<Value> values;
  int32_t value = 0;
  int32_t default_value = 0;
  int32_t minimum = 0;
  int32_t maximum = 0;
  int32_t step = 1;
  // Reset targets must be value rows in this page or its descendants.
  std::vector<Id> reset_rows;
  std::string confirmation;
  std::string yes;
  std::string no;
};
struct Page {
  Id id = 0;
  std::string label;
  std::string hints;
  std::vector<Row> rows;
};
struct Definition {
  std::string title;
  Id root = 0;
  std::vector<Page> pages;
};

enum class Input { Up, Down, Left, Right, Activate, Back };
enum class Status { Ok, Boundary, Closed, Busy, Invalid, Stale, Rejected };
enum class Event {
  None, Opened, SectionChanged, Returned, SelectionChanged, ValueChanged,
  ConfirmationOpened, ConfirmationChanged, ConfirmationClosed, ResetApplied,
  ActionApplied, CloseRequested, NoticeChanged
};
enum class CommandKind { Adjust, Reset, Action };
struct Change {
  Id row = 0;
  int32_t before = 0;
  int32_t after = 0;
};
struct Command {
  uint64_t generation = 0;
  CommandKind kind = CommandKind::Adjust;
  Id row = 0;
  std::vector<Change> changes;
};
struct Result {
  Status status = Status::Ok;
  Event event = Event::None;
  std::optional<Command> command;
};
struct RowView {
  Id id = 0;
  RowKind kind = RowKind::Action;
  std::string label;
  std::string value;
  std::string description;
  int32_t numeric_value = 0;
};
struct View {
  bool open = false;
  bool pending = false;
  Id page = 0;
  std::string title;
  std::string section;
  std::string hints;
  std::vector<RowView> rows;
  // One-based; zero means an empty page. All rows are navigable.
  size_t position = 0;
  bool confirming = false;
  bool confirm_yes = false;
  std::string confirmation;
  std::string yes;
  std::string no;
  // Integration-owned, localized status, also rendered during confirmation.
  std::string notice;
};

// Single-owner model; no guest, renderer, speech, persistence or thread calls.
// Configure copies validated definitions and is allowed only while closed.
// Opened alone introduces the title. SectionChanged/Returned introduce the
// current section and item, never the title. Other movement/value events
// introduce the item alone. The presenter owns speech queuing and hint policy.
class Menu {
 public:
  bool Configure(const Definition& definition);
  Result Open();
  void Close();
  Result Handle(Input input);
  // Accept only after the entire command was applied and autosave scheduled.
  // A rejected batch must leave the backend unchanged. Disk completion belongs
  // to the adapter. While a command is pending, inputs return Busy. Closing
  // invalidates it. Tokens never repeat across reconfiguration/reopening.
  Result Resolve(uint64_t generation, bool accepted);
  // Copy externally committed values without changing focus, confirmation or
  // defaults. Atomic validation rejects duplicate/unknown/non-value rows and
  // unsupported values. Choice replacements apply in the same atomic batch and
  // must retain the unchanged default and resulting current value. Only Choice
  // rows accept replacements. Pending commands return Busy; no event is made.
  Status SynchronizeValues(const std::vector<std::pair<Id, int32_t>>& values,
                           const std::vector<ChoiceUpdate>& choices = {});
  View Read() const;

 private:
  struct Frame { size_t page = 0; size_t selected = 0; };
  Row* FindRow(Id id);
  const Row* FindRow(Id id) const;
  Row* Selected();
  Result Request(CommandKind kind, Row& row, std::vector<Change> changes);
  Definition definition_;
  std::vector<Frame> stack_;
  std::optional<Command> pending_;
  uint64_t generation_ = 0;
  bool configured_ = false;
  bool confirming_ = false;
  bool confirm_yes_ = false;
};

}  // namespace brawlers::settings
