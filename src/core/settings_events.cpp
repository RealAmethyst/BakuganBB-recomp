#include "settings_events.h"

namespace brawlers::settings {

ScreenEvent Events::Make(const char* source, const char* kind) const {
  ScreenEvent result;
  result.source = source;
  result.kind = kind;
  result.id = identity_;
  result.locale = locale_;
  return result;
}

ScreenEvent Events::Item(const char* kind, const View& view) const {
  auto result = Make("settings", kind);
  result.section = view.section;
  if (view.position && view.position <= view.rows.size()) {
    const auto& row = view.rows[view.position - 1];
    result.label = row.label;
    result.value = row.value;
    result.description = row.description;
    if (!view.notice.empty()) {
      if (!result.description.empty()) result.description += " ";
      result.description += view.notice;
    }
    result.position = static_cast<uint32_t>(view.position);
    result.total = static_cast<uint32_t>(view.rows.size());
  }
  if (!view.hints.empty()) result.hints.push_back(view.hints);
  return result;
}

std::vector<ScreenEvent> Events::Present(uint64_t identity, uint32_t locale,
                                       Event event, const View& view) {
  std::vector<ScreenEvent> result;
  if (!identity || locale > 7 || !view.open || view.rows.size() > 256 ||
      view.position > view.rows.size()) return result;
  if (event == Event::Opened) {
    if (identity_) return result;
    identity_ = identity;
    locale_ = locale;
    auto opening = Item("opened", view);
    opening.title = view.title;
    result.push_back(std::move(opening));
    return result;
  }
  if (identity_ != identity) return result;
  // A native child may have hidden asynchronous model changes. Reconcile the
  // actual confirmation lifetime before resuming its parent or current choice.
  if (event == Event::Returned && view.confirming && !confirming_)
    event = Event::ConfirmationOpened;
  if (confirming_ && view.confirming &&
      (event == Event::NoticeChanged || event == Event::Returned)) {
    auto focused = Make("settings_confirmation", event == Event::Returned ? "resumed" : "focused");
    focused.label = view.confirm_yes ? view.yes : view.no;
    focused.description = view.notice;
    focused.position = view.confirm_yes ? 1 : 2;
    focused.total = 2;
    result.push_back(std::move(focused));
    return result;
  }
  if (event == Event::ConfirmationOpened && view.confirming && !confirming_) {
    confirming_ = true;
    auto opening = Make("settings_confirmation", "opened");
    opening.body = view.confirmation;
    opening.label = view.confirm_yes ? view.yes : view.no;
    opening.position = view.confirm_yes ? 1 : 2;
    opening.total = 2;
    result.push_back(std::move(opening));
  } else if (event == Event::ConfirmationChanged && view.confirming && confirming_) {
    auto focused = Make("settings_confirmation", "focused");
    focused.label = view.confirm_yes ? view.yes : view.no;
    focused.position = view.confirm_yes ? 1 : 2;
    focused.total = 2;
    result.push_back(std::move(focused));
  } else if ((event == Event::ConfirmationClosed || event == Event::ResetApplied ||
              event == Event::Returned) &&
             !view.confirming && confirming_) {
    confirming_ = false;
    result.push_back(Make("settings_confirmation", "closed"));
    result.push_back(Item("resumed", view));
  } else if (!confirming_) {
    switch (event) {
      case Event::Returned: case Event::SectionChanged:
        result.push_back(Item("resumed", view));
        break;
      case Event::SelectionChanged: case Event::ValueChanged: case Event::ActionApplied:
      case Event::NoticeChanged:
        result.push_back(Item("focused", view));
        break;
      default: break;
    }
  }
  return result;
}

std::vector<ScreenEvent> Events::Closed() {
  std::vector<ScreenEvent> result;
  if (!identity_) return result;
  if (confirming_) result.push_back(Make("settings_confirmation", "closed"));
  result.push_back(Make("settings", "closed"));
  confirming_ = false;
  identity_ = 0;
  return result;
}

}  // namespace brawlers::settings
