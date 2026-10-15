#include "settings_menu.h"

#include <cassert>
#include <limits>
#include <string>

using namespace brawlers::settings;

namespace {

Row Link(Id id, Id target) {
  Row row;
  row.id = id;
  row.kind = RowKind::Page;
  row.page = target;
  row.label = "Synthetic category";
  return row;
}

Row Toggle(Id id) {
  Row row;
  row.id = id;
  row.kind = RowKind::Toggle;
  row.label = "Synthetic toggle";
  row.description = "Synthetic description";
  row.values = {{0, "Synthetic disabled"}, {1, "Synthetic enabled"}};
  row.value = 1;
  row.default_value = 1;
  return row;
}

Row Range(Id id, int32_t minimum, int32_t maximum, int32_t step) {
  Row row;
  row.id = id;
  row.kind = RowKind::Range;
  row.label = "Synthetic range";
  row.minimum = minimum;
  row.maximum = maximum;
  row.step = step;
  row.value = maximum;
  row.default_value = maximum;
  for (int32_t v = minimum; v <= maximum; v += step)
    row.values.push_back({v, "Synthetic value " + std::to_string(v)});
  return row;
}

Row Reset(Id id, std::vector<Id> targets) {
  Row row;
  row.id = id;
  row.kind = RowKind::Reset;
  row.label = "Synthetic defaults";
  row.reset_rows = std::move(targets);
  row.confirmation = "Synthetic confirmation";
  row.yes = "Synthetic yes";
  row.no = "Synthetic no";
  return row;
}

Definition Fixture() {
  Definition def;
  def.title = "Synthetic title";
  def.root = 1;
  def.pages = {
      {1, "", "Synthetic root hints", {Link(10, 2), Link(11, 3), Link(12, 5)}},
      {2, "Synthetic controls", "Synthetic control hints", {Toggle(20), Range(21, 1, 10, 1), Reset(22, {20, 21})}},
      {3, "Synthetic accessibility", "Synthetic hints", {Link(30, 4), Reset(31, {40, 41})}},
      {4, "Synthetic cues", "Synthetic cue hints", {Toggle(40), Range(41, 0, 100, 10)}},
      {5, "Synthetic empty", "", {}}};
  return def;
}

void Apply(Menu& menu, Input input, int32_t expected) {
  const auto result = menu.Handle(input);
  assert(result.status == Status::Ok && result.command);
  assert(result.command->kind == CommandKind::Adjust);
  assert(result.command->changes.size() == 1);
  assert(result.command->changes[0].after == expected);
  assert(menu.Read().pending);
  assert(menu.Resolve(result.command->generation, true).event == Event::ValueChanged);
  assert(!menu.Read().pending);
}

void NavigationAndLifetime() {
  Menu menu;
  assert(menu.Open().status == Status::Invalid);
  auto fixture = Fixture();
  assert(menu.Configure(fixture));
  fixture.title = "Changed caller storage";
  fixture.pages[0].rows[0].label = "Changed caller row";
  assert(menu.Open().event == Event::Opened);
  assert(menu.Read().title == "Synthetic title");
  assert(menu.Read().rows[0].label == "Synthetic category");
  assert(menu.Read().position == 1 && menu.Read().rows.size() == 3);
  assert(menu.Open().status == Status::Busy);
  assert(!menu.Configure(Fixture()));
  assert(menu.Handle(Input::Up).status == Status::Boundary);
  assert(menu.Handle(Input::Left).status == Status::Boundary);
  assert(menu.Handle(Input::Down).event == Event::SelectionChanged);
  assert(menu.Handle(Input::Activate).event == Event::SectionChanged);
  assert(menu.Read().page == 3 && menu.Read().section == "Synthetic accessibility");
  assert(menu.Handle(Input::Activate).event == Event::SectionChanged);
  assert(menu.Read().page == 4);
  Apply(menu, Input::Activate, 0);
  assert(menu.Handle(Input::Back).event == Event::Returned);
  assert(menu.Read().page == 3 && menu.Read().position == 1);
  assert(menu.Handle(Input::Back).event == Event::Returned);
  assert(menu.Read().position == 2 && menu.Read().page == 1);
  assert(menu.Handle(Input::Down).event == Event::SelectionChanged);
  assert(menu.Handle(Input::Down).status == Status::Boundary);
  assert(menu.Handle(Input::Activate).event == Event::SectionChanged);
  assert(menu.Read().rows.empty() && menu.Read().position == 0);
  for (Input input : {Input::Up, Input::Down, Input::Left, Input::Right, Input::Activate})
    assert(menu.Handle(input).status == Status::Boundary);
  assert(menu.Handle(Input::Back).event == Event::Returned);
  assert(menu.Read().position == 3);
  assert(menu.Handle(Input::Back).event == Event::CloseRequested);
  assert(menu.Read().open);  // Actual native close belongs to the owner.
  menu.Close();
  assert(!menu.Read().open);
  assert(menu.Handle(Input::Activate).status == Status::Closed);
  assert(menu.Open().event == Event::Opened);
  assert(menu.Read().position == 1);
  menu.Close();
}

void AdjustmentsAndRejection() {
  Menu menu;
  assert(menu.Configure(Fixture()));
  menu.Open();
  menu.Handle(Input::Activate);
  auto change = menu.Handle(Input::Left);
  assert(change.command && change.command->changes[0].before == 1);
  assert(menu.Read().rows[0].numeric_value == 1);
  assert(menu.Handle(Input::Right).status == Status::Busy);
  assert(menu.Handle(Input::Back).status == Status::Busy);
  assert(menu.Resolve(change.command->generation + 1, true).status == Status::Stale);
  assert(menu.Read().pending);
  assert(menu.Resolve(change.command->generation, false).status == Status::Rejected);
  assert(menu.Read().rows[0].numeric_value == 1);
  assert(menu.Resolve(change.command->generation, true).status == Status::Stale);
  Apply(menu, Input::Left, 0);
  assert(menu.Handle(Input::Left).status == Status::Boundary);
  Apply(menu, Input::Activate, 1);
  assert(menu.Handle(Input::Right).status == Status::Boundary);
  menu.Handle(Input::Down);
  assert(menu.Handle(Input::Activate).status == Status::Boundary);
  for (int32_t n = 9; n >= 1; --n) Apply(menu, Input::Left, n);
  assert(menu.Handle(Input::Left).status == Status::Boundary);
  for (int32_t n = 2; n <= 10; ++n) Apply(menu, Input::Right, n);
  assert(menu.Handle(Input::Right).status == Status::Boundary);
  assert(menu.Read().rows[1].value == "Synthetic value 10");
  change = menu.Handle(Input::Left);
  menu.Close();
  assert(menu.Resolve(change.command->generation, true).status == Status::Stale);
  assert(menu.Configure(Fixture()));
  menu.Open();
  menu.Handle(Input::Activate);
  const auto newer = menu.Handle(Input::Left);
  assert(newer.command->generation > change.command->generation);
  assert(menu.Resolve(change.command->generation, true).status == Status::Stale);
  assert(menu.Resolve(newer.command->generation, true).event == Event::ValueChanged);
}

void ResetConfirmation() {
  Menu menu;
  assert(menu.Configure(Fixture()));
  menu.Open();
  menu.Handle(Input::Activate);
  Apply(menu, Input::Left, 0);
  menu.Handle(Input::Down);
  Apply(menu, Input::Left, 9);
  menu.Handle(Input::Down);
  assert(menu.Handle(Input::Activate).event == Event::ConfirmationOpened);
  assert(menu.Read().confirming && !menu.Read().confirm_yes);
  assert(menu.Read().confirmation == "Synthetic confirmation");
  assert(menu.Read().yes == "Synthetic yes" && menu.Read().no == "Synthetic no");
  assert(menu.Handle(Input::Activate).event == Event::ConfirmationClosed);
  assert(menu.Read().rows[0].numeric_value == 0);
  menu.Handle(Input::Activate);
  assert(menu.Handle(Input::Left).event == Event::ConfirmationChanged);
  assert(menu.Handle(Input::Left).status == Status::Boundary);
  assert(menu.Handle(Input::Right).event == Event::ConfirmationChanged);
  assert(menu.Handle(Input::Back).event == Event::ConfirmationClosed);
  menu.Handle(Input::Activate);
  assert(!menu.Read().confirm_yes);
  menu.Handle(Input::Left);
  const auto command = menu.Handle(Input::Activate);
  assert(command.command->kind == CommandKind::Reset);
  assert(command.command->changes.size() == 2);
  assert(command.command->changes[0].row == 20 && command.command->changes[0].after == 1);
  assert(command.command->changes[1].row == 21 && command.command->changes[1].after == 10);
  assert(menu.Resolve(command.command->generation, false).status == Status::Rejected);
  assert(menu.Read().confirming && menu.Read().rows[0].numeric_value == 0);
  const auto retry = menu.Handle(Input::Activate);
  assert(menu.Resolve(retry.command->generation, true).event == Event::ResetApplied);
  assert(!menu.Read().confirming && menu.Read().position == 3);
  assert(menu.Read().rows[0].numeric_value == 1 && menu.Read().rows[1].numeric_value == 10);
  // Resetting an already-default group remains an explicit, empty transaction.
  menu.Handle(Input::Activate);
  menu.Handle(Input::Left);
  const auto unchanged = menu.Handle(Input::Activate);
  assert(unchanged.command && unchanged.command->changes.empty());
  assert(menu.Resolve(unchanged.command->generation, true).event == Event::ResetApplied);
  menu.Handle(Input::Back);
  menu.Handle(Input::Down);
  menu.Handle(Input::Activate);
  menu.Handle(Input::Activate);
  Apply(menu, Input::Left, 0);
  menu.Handle(Input::Down);
  for (int32_t n = 90; n >= 0; n -= 10) Apply(menu, Input::Left, n);
  assert(menu.Handle(Input::Left).status == Status::Boundary);
  menu.Handle(Input::Back);
  menu.Handle(Input::Down);
  menu.Handle(Input::Activate);
  menu.Handle(Input::Left);
  auto nested = menu.Handle(Input::Activate);
  assert(nested.command->changes.size() == 2);
  assert(nested.command->changes[0].row == 40 && nested.command->changes[1].row == 41);
  menu.Resolve(nested.command->generation, true);
  menu.Handle(Input::Up);
  menu.Handle(Input::Activate);
  assert(menu.Read().rows[0].numeric_value == 1 && menu.Read().rows[1].numeric_value == 100);
}

void ChoicesActionsAndSingleton() {
  Definition def;
  def.root = 1;
  Row choice;
  choice.id = 10;
  choice.kind = RowKind::Choice;
  choice.label = "Synthetic mode";
  choice.values = {{27, "Synthetic first"}, {-3, "Synthetic second"}, {14, "Synthetic third"}};
  choice.value = choice.default_value = 27;
  def.pages = {{1, "", "", {choice}}};
  Menu menu;
  assert(menu.Configure(def));
  menu.Open();
  assert(menu.Read().position == 1 && menu.Read().rows.size() == 1);
  assert(menu.Handle(Input::Down).status == Status::Boundary);
  Apply(menu, Input::Right, -3);
  Apply(menu, Input::Right, 14);
  assert(menu.Handle(Input::Right).status == Status::Boundary);
  Apply(menu, Input::Left, -3);
  menu.Close();
  Row action;
  action.id = 90;
  action.label = "Synthetic action";
  def.pages[0].rows = {action};
  assert(menu.Configure(def));
  menu.Open();
  auto result = menu.Handle(Input::Activate);
  assert(result.command->kind == CommandKind::Action && result.command->changes.empty());
  assert(menu.Resolve(result.command->generation, true).event == Event::ActionApplied);
  assert(menu.Handle(static_cast<Input>(100)).status == Status::Invalid);
  menu.Close();
  def.pages[0].rows = {Range(91, 5, 5, 3)};
  assert(menu.Configure(def));
  menu.Open();
  assert(menu.Handle(Input::Left).status == Status::Boundary);
  assert(menu.Handle(Input::Right).status == Status::Boundary);
  assert(menu.Read().rows[0].numeric_value == 5);
}

void ExternalValuesPreserveState() {
  Menu menu;
  assert(menu.SynchronizeValues({}) == Status::Invalid);
  assert(menu.Configure(Fixture()));
  // A refresh may arrive before the native opening is presented.
  assert(menu.SynchronizeValues({{20, 0}, {41, 30}}) == Status::Ok);
  menu.Open();
  menu.Handle(Input::Activate);
  assert(menu.Read().rows[0].numeric_value == 0);
  menu.Handle(Input::Down);
  assert(menu.SynchronizeValues({{20, 1}, {21, 5}}) == Status::Ok);
  assert(menu.Read().page == 2 && menu.Read().position == 2);
  assert(menu.Read().rows[1].value == "Synthetic value 5");
  // Every batch validates completely before any earlier valid entry changes.
  for (const auto& invalid : std::vector<std::vector<std::pair<Id, int32_t>>>{
      {{20, 0}, {999, 1}}, {{20, 0}, {21, 11}}, {{20, 0}, {10, 0}},
      {{20, 0}, {22, 0}}, {{20, 0}, {20, 1}}, {{20, 0}, {41, 35}}}) {
    assert(menu.SynchronizeValues(invalid) == Status::Invalid);
    assert(menu.Read().rows[0].numeric_value == 1);
    assert(menu.Read().rows[1].numeric_value == 5);
  }
  const auto command = menu.Handle(Input::Right);
  assert(command.command && command.command->changes[0].before == 5);
  assert(menu.SynchronizeValues({{21, 3}}) == Status::Busy);
  assert(menu.SynchronizeValues({}) == Status::Busy);
  assert(menu.Read().pending && menu.Read().rows[1].numeric_value == 5);
  assert(menu.Resolve(command.command->generation, true).event == Event::ValueChanged);
  assert(menu.Read().rows[1].numeric_value == 6);
  menu.Handle(Input::Down);
  menu.Handle(Input::Activate);
  menu.Handle(Input::Left);
  assert(menu.SynchronizeValues({{20, 0}, {21, 4}}) == Status::Ok);
  assert(menu.Read().confirming && menu.Read().confirm_yes && menu.Read().position == 3);
  const auto reset = menu.Handle(Input::Activate);
  assert(reset.command->generation == command.command->generation + 1);
  assert(reset.command->changes.size() == 2);
  assert(reset.command->changes[0].before == 0 && reset.command->changes[0].after == 1);
  assert(reset.command->changes[1].before == 4 && reset.command->changes[1].after == 10);
  assert(menu.Resolve(reset.command->generation, true).event == Event::ResetApplied);
  menu.Handle(Input::Back);
  assert(menu.Read().page == 1 && menu.Read().position == 1);
  menu.Handle(Input::Down);
  menu.Handle(Input::Activate);
  menu.Handle(Input::Activate);
  assert(menu.Read().rows[1].numeric_value == 30);
  assert(menu.SynchronizeValues({}) == Status::Ok);
  menu.Close();
  assert(menu.SynchronizeValues({{40, 0}}) == Status::Ok);
  assert(!menu.Read().open);
}

void InvalidDefinitions() {
  const auto invalid = [](Definition definition) { Menu menu; assert(!menu.Configure(definition)); };
  auto def = Fixture();
  def.pages[1].rows[1].value = 0;
  invalid(def);
  def = Fixture(); def.pages[1].rows[1].default_value = 11; invalid(def);
  def = Fixture(); def.pages[1].rows[1].step = 0; invalid(def);
  def = Fixture(); def.pages[1].rows[1].maximum = 11; invalid(def);
  def = Fixture(); def.pages[1].rows[1].values[0].value = 2; invalid(def);
  def = Fixture(); def.pages[1].rows[0].values[0].value = -1; invalid(def);
  def = Fixture(); def.pages[1].rows[0].values[0].label.clear(); invalid(def);
  def = Fixture(); def.pages[0].rows[0].page = 999; invalid(def);
  def = Fixture(); def.pages[0].rows[0].page = 1; invalid(def);
  def = Fixture(); def.pages[0].rows[0].page = 3; invalid(def);
  def = Fixture(); def.pages[1].rows[0].id = 10; invalid(def);
  def = Fixture(); def.pages[0].id = 2; invalid(def);
  def = Fixture(); def.pages[1].rows[2].reset_rows = {40}; invalid(def);
  def = Fixture(); def.pages[1].rows[2].reset_rows = {20, 20}; invalid(def);
  def = Fixture(); def.pages[1].rows[2].reset_rows = {22}; invalid(def);
  def = Fixture(); def.pages[1].rows[2].reset_rows = {999}; invalid(def);
  def = Fixture(); def.pages[1].rows[2].yes.clear(); invalid(def);
  def = Fixture(); def.pages[1].rows[0].kind = static_cast<RowKind>(100); invalid(def);
  def = Fixture(); def.title = std::string("a\0b", 3); invalid(def);
  def = Fixture(); def.pages[0].rows[0].label.assign(2049, 'x'); invalid(def);
  def = Fixture(); def.pages[0].rows[0].page = 5; invalid(def);
  // An unreachable cycle, including an earlier reset that targets that cycle,
  // must fail without looping during descendant validation.
  def = Fixture();
  def.pages[0].rows.erase(def.pages[0].rows.begin() + 1);
  def.pages[3].rows.push_back(Link(42, 3));
  def.pages[1].rows[2].reset_rows = {40};
  invalid(def);
  def = Fixture(); def.pages[1].rows[1].minimum = std::numeric_limits<int32_t>::min();
  def.pages[1].rows[1].maximum = std::numeric_limits<int32_t>::max(); invalid(def);
  Menu menu;
  assert(menu.Configure(Fixture()));
  assert(!menu.Configure(def));
  assert(menu.Open().event == Event::Opened);  // Invalid replacement preserves valid data.
}

}  // namespace

int main() {
  NavigationAndLifetime();
  AdjustmentsAndRejection();
  ResetConfirmation();
  ChoicesActionsAndSingleton();
  InvalidDefinitions();
  ExternalValuesPreserveState();
}
