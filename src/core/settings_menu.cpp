#include "settings_menu.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace brawlers::settings {
namespace {

bool IsValue(RowKind kind) {
  return kind == RowKind::Toggle || kind == RowKind::Range || kind == RowKind::Choice;
}

bool Text(const std::string& text, bool required = false) {
  return (!required || !text.empty()) && text.size() <= 2048 &&
         text.find('\0') == std::string::npos;
}

bool Validate(const Definition& definition) {
  if (!Text(definition.title) || definition.root == 0 || definition.pages.empty() ||
      definition.pages.size() > 32)
    return false;
  std::unordered_map<Id, size_t> pages;
  std::unordered_map<Id, std::pair<size_t, const Row*>> rows;
  size_t value_count = 0;
  for (size_t p = 0; p < definition.pages.size(); ++p) {
    const auto& page = definition.pages[p];
    if (!page.id || !pages.emplace(page.id, p).second || !Text(page.label) ||
        !Text(page.hints) || page.rows.size() > 256)
      return false;
    for (const auto& row : page.rows) {
      if (!row.id || !rows.emplace(row.id, std::make_pair(p, &row)).second ||
          rows.size() > 256 || !Text(row.label, true) || !Text(row.description) ||
          !Text(row.confirmation) || !Text(row.yes) || !Text(row.no))
        return false;
      switch (row.kind) {
        case RowKind::Page: case RowKind::Toggle: case RowKind::Range:
        case RowKind::Choice: case RowKind::Action: case RowKind::Reset: break;
        default: return false;
      }
      if ((row.kind == RowKind::Page) != (row.page != 0))
        return false;
      if (row.kind != RowKind::Reset && (!row.reset_rows.empty() ||
          !row.confirmation.empty() || !row.yes.empty() || !row.no.empty()))
        return false;
      if (!IsValue(row.kind)) {
        if (!row.values.empty()) return false;
        continue;
      }
      if (row.values.empty() || row.values.size() > 1024) return false;
      value_count += row.values.size();
      if (value_count > 16384) return false;
      std::unordered_set<int32_t> values;
      for (const auto& value : row.values) {
        if (!values.insert(value.value).second || !Text(value.label, true)) return false;
      }
      if (!values.count(row.value) || !values.count(row.default_value)) return false;
      if (row.kind == RowKind::Toggle &&
          (row.values.size() != 2 || row.values[0].value != 0 || row.values[1].value != 1))
        return false;
      if (row.kind == RowKind::Range) {
        const int64_t span = int64_t{row.maximum} - row.minimum;
        if (row.step <= 0 || span < 0 || span % row.step ||
            span / row.step + 1 != static_cast<int64_t>(row.values.size())) return false;
        for (size_t i = 0; i < row.values.size(); ++i) {
          if (row.values[i].value != int64_t{row.minimum} + static_cast<int64_t>(i) * row.step)
            return false;
        }
      }
    }
  }
  const auto root = pages.find(definition.root);
  if (root == pages.end()) return false;
  std::vector<size_t> parent(definition.pages.size(), definition.pages.size());
  for (size_t p = 0; p < definition.pages.size(); ++p) {
    for (const auto& row : definition.pages[p].rows) {
      if (row.kind != RowKind::Page) continue;
      const auto target = pages.find(row.page);
      if (target == pages.end() || target->second == root->second ||
          parent[target->second] != definition.pages.size()) return false;
      parent[target->second] = p;
    }
  }
  for (size_t p = 0; p < definition.pages.size(); ++p) {
    size_t at = p;
    size_t steps = 0;
    while (at != root->second) {
      if (at == definition.pages.size() || ++steps > definition.pages.size()) return false;
      at = parent[at];
    }
  }
  for (size_t p = 0; p < definition.pages.size(); ++p) {
    for (const auto& row : definition.pages[p].rows) {
      if (row.kind != RowKind::Reset) continue;
      if (row.reset_rows.empty() || row.reset_rows.size() > 256 ||
          !Text(row.confirmation, true) || !Text(row.yes, true) || !Text(row.no, true))
        return false;
      std::unordered_set<Id> targets;
      for (Id id : row.reset_rows) {
        auto found = rows.find(id);
        if (found == rows.end() || !IsValue(found->second.second->kind) ||
            !targets.insert(id).second) return false;
        size_t owner = found->second.first;
        while (owner != p && owner != definition.pages.size()) owner = parent[owner];
        if (owner != p) return false;
      }
    }
  }
  return true;
}

Result Outcome(Status status, Event event = Event::None) {
  return {status, event, std::nullopt};
}

}  // namespace

bool Menu::Configure(const Definition& definition) {
  if (!stack_.empty() || !Validate(definition)) return false;
  Definition copy = definition;
  definition_ = std::move(copy);
  configured_ = true;
  return true;
}

Result Menu::Open() {
  if (!configured_) return Outcome(Status::Invalid);
  if (!stack_.empty()) return Outcome(Status::Busy);
  const auto root = std::find_if(definition_.pages.begin(), definition_.pages.end(),
      [&](const Page& page) { return page.id == definition_.root; });
  stack_.push_back({static_cast<size_t>(root - definition_.pages.begin()), 0});
  return Outcome(Status::Ok, Event::Opened);
}

void Menu::Close() {
  stack_.clear();
  pending_.reset();
  confirming_ = false;
  confirm_yes_ = false;
}

Row* Menu::FindRow(Id id) {
  for (auto& page : definition_.pages)
    for (auto& row : page.rows)
      if (row.id == id) return &row;
  return nullptr;
}

const Row* Menu::FindRow(Id id) const {
  for (const auto& page : definition_.pages)
    for (const auto& row : page.rows)
      if (row.id == id) return &row;
  return nullptr;
}

Row* Menu::Selected() {
  if (stack_.empty()) return nullptr;
  auto& frame = stack_.back();
  auto& rows = definition_.pages[frame.page].rows;
  return rows.empty() ? nullptr : &rows[frame.selected];
}

Result Menu::Request(CommandKind kind, Row& row, std::vector<Change> changes) {
  if (generation_ == std::numeric_limits<uint64_t>::max()) return Outcome(Status::Invalid);
  Command command{generation_ + 1, kind, row.id, std::move(changes)};
  Result result{Status::Ok, Event::None, command};
  pending_ = std::move(command);
  ++generation_;
  return result;
}

Result Menu::Handle(Input input) {
  if (stack_.empty()) return Outcome(Status::Closed);
  if (pending_) return Outcome(Status::Busy);
  switch (input) {
    case Input::Up: case Input::Down: case Input::Left: case Input::Right:
    case Input::Activate: case Input::Back: break;
    default: return Outcome(Status::Invalid);
  }
  auto* row = Selected();
  if (confirming_) {
    if (input == Input::Back || (input == Input::Activate && !confirm_yes_)) {
      confirming_ = false;
      return Outcome(Status::Ok, Event::ConfirmationClosed);
    }
    if (input == Input::Left || input == Input::Right) {
      const bool yes = input == Input::Left;
      if (confirm_yes_ == yes) return Outcome(Status::Boundary);
      confirm_yes_ = yes;
      return Outcome(Status::Ok, Event::ConfirmationChanged);
    }
    if (input == Input::Activate) {
      std::vector<Change> changes;
      for (Id id : row->reset_rows) {
        const auto* target = FindRow(id);
        if (target->value != target->default_value)
          changes.push_back({id, target->value, target->default_value});
      }
      return Request(CommandKind::Reset, *row, std::move(changes));
    }
    return Outcome(Status::Boundary);
  }
  if (input == Input::Back) {
    if (stack_.size() == 1) return Outcome(Status::Ok, Event::CloseRequested);
    stack_.pop_back();
    return Outcome(Status::Ok, Event::Returned);
  }
  if (!row) return Outcome(Status::Boundary);
  auto& frame = stack_.back();
  if (input == Input::Up || input == Input::Down) {
    const size_t count = definition_.pages[frame.page].rows.size();
    if ((input == Input::Up && frame.selected == 0) ||
        (input == Input::Down && frame.selected + 1 == count)) return Outcome(Status::Boundary);
    if (input == Input::Up) --frame.selected; else ++frame.selected;
    return Outcome(Status::Ok, Event::SelectionChanged);
  }
  if (input == Input::Activate) {
    if (row->kind == RowKind::Page) {
      const auto page = std::find_if(definition_.pages.begin(), definition_.pages.end(),
          [&](const Page& candidate) { return candidate.id == row->page; });
      stack_.push_back({static_cast<size_t>(page - definition_.pages.begin()), 0});
      return Outcome(Status::Ok, Event::SectionChanged);
    }
    if (row->kind == RowKind::Reset) {
      confirming_ = true;
      confirm_yes_ = false;
      return Outcome(Status::Ok, Event::ConfirmationOpened);
    }
    if (row->kind == RowKind::Action) return Request(CommandKind::Action, *row, {});
    if (row->kind != RowKind::Toggle) return Outcome(Status::Boundary);
  }
  if (!IsValue(row->kind)) return Outcome(Status::Boundary);
  const auto value = std::find_if(row->values.begin(), row->values.end(),
      [&](const Value& candidate) { return candidate.value == row->value; });
  size_t index = static_cast<size_t>(value - row->values.begin());
  if (input == Input::Activate) index = 1 - index;
  else if (input == Input::Left) {
    if (index == 0) return Outcome(Status::Boundary);
    --index;
  } else {
    if (index + 1 == row->values.size()) return Outcome(Status::Boundary);
    ++index;
  }
  return Request(CommandKind::Adjust, *row, {{row->id, row->value, row->values[index].value}});
}

Result Menu::Resolve(uint64_t generation, bool accepted) {
  if (!pending_ || pending_->generation != generation) return Outcome(Status::Stale);
  const auto command = std::move(*pending_);
  pending_.reset();
  if (!accepted) return Outcome(Status::Rejected);
  for (const auto& change : command.changes) FindRow(change.row)->value = change.after;
  if (command.kind == CommandKind::Reset) {
    confirming_ = false;
    return Outcome(Status::Ok, Event::ResetApplied);
  }
  return Outcome(Status::Ok, command.kind == CommandKind::Adjust ?
      Event::ValueChanged : Event::ActionApplied);
}

Status Menu::SynchronizeValues(const std::vector<std::pair<Id, int32_t>>& values,
                               const std::vector<ChoiceUpdate>& choices) {
  if (pending_) return Status::Busy;
  if (!configured_ || values.size() > 256 || choices.size() > 256) return Status::Invalid;
  auto updated = definition_;
  const auto find = [&](Id id) -> Row* {
    for (auto& page : updated.pages)
      for (auto& row : page.rows)
        if (row.id == id) return &row;
    return nullptr;
  };
  std::unordered_set<Id> seen;
  for (const auto& choice : choices) {
    auto* row = find(choice.row);
    if (!row || row->kind != RowKind::Choice || !seen.insert(choice.row).second ||
        choice.values.empty() || choice.values.size() > 1024) return Status::Invalid;
    row->values = choice.values;
  }
  seen.clear();
  for (const auto& [id, value] : values) {
    auto* row = find(id);
    if (!row || !IsValue(row->kind) || !seen.insert(id).second)
      return Status::Invalid;
    row->value = value;
  }
  if (!Validate(updated)) return Status::Invalid;
  definition_ = std::move(updated);
  return Status::Ok;
}

View Menu::Read() const {
  View view;
  if (stack_.empty()) return view;
  const auto& frame = stack_.back();
  const auto& page = definition_.pages[frame.page];
  view.open = true;
  view.pending = pending_.has_value();
  view.page = page.id;
  view.title = definition_.title;
  view.section = page.label;
  view.hints = page.hints;
  view.position = page.rows.empty() ? 0 : frame.selected + 1;
  for (const auto& row : page.rows) {
    RowView item{row.id, row.kind, row.label, {}, row.description, row.value};
    if (IsValue(row.kind)) {
      const auto value = std::find_if(row.values.begin(), row.values.end(),
          [&](const Value& candidate) { return candidate.value == row.value; });
      item.value = value->label;
    }
    view.rows.push_back(std::move(item));
  }
  view.confirming = confirming_;
  view.confirm_yes = confirm_yes_;
  if (confirming_) {
    const auto& row = page.rows[frame.selected];
    view.confirmation = row.confirmation;
    view.yes = row.yes;
    view.no = row.no;
  }
  return view;
}

}  // namespace brawlers::settings
