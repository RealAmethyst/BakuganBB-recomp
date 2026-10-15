#include "settings_events.h"

#include <cassert>

using namespace brawlers::settings;

int main() {
  Events events;
  View view;
  view.open = true;
  view.title = "Synthetic settings";
  view.hints = "Synthetic navigation";
  view.rows = {{11, RowKind::Range, "Synthetic volume", "20%", "Synthetic detail", 20}};
  view.position = 1;
  assert(events.Present(7, 1, Event::SelectionChanged, view).empty());
  auto result = events.Present(7, 1, Event::Opened, view);
  assert(result.size() == 1 && result[0].source == "settings");
  assert(result[0].title == view.title && result[0].value == "20%");
  assert(result[0].description == "Synthetic detail");
  assert(result[0].position == 1 && result[0].total == 1);
  assert(events.Present(7, 1, Event::Opened, view).empty());
  assert(events.Present(8, 1, Event::ValueChanged, view).empty());

  view.section = "Synthetic audio";
  result = events.Present(7, 1, Event::SectionChanged, view);
  assert(result.size() == 1 && result[0].kind == "resumed");
  assert(result[0].title.empty() && result[0].section == view.section);
  view.notice = "Synthetic save failure";
  result = events.Present(7, 1, Event::NoticeChanged, view);
  assert(result.size() == 1 && result[0].kind == "focused");
  assert(result[0].description == "Synthetic detail Synthetic save failure");
  assert(result[0].position == 1 && result[0].total == 1);
  view.notice.clear();

  view.confirming = true;
  view.confirmation = "Synthetic reset question";
  view.yes = "Confirm";
  view.no = "Cancel";
  result = events.Present(7, 1, Event::ConfirmationOpened, view);
  assert(result.size() == 1 && result[0].source == "settings_confirmation");
  assert(result[0].body == view.confirmation && result[0].label == "Cancel");
  assert(result[0].position == 2 && result[0].total == 2);
  view.notice = "Synthetic reset failure";
  result = events.Present(7, 1, Event::NoticeChanged, view);
  assert(result.size() == 1 && result[0].source == "settings_confirmation");
  assert(result[0].label == "Cancel" && result[0].description == view.notice);
  assert(result[0].position == 2 && result[0].total == 2);
  view.notice.clear();
  assert(events.Present(7, 1, Event::SelectionChanged, view).empty());
  view.confirm_yes = true;
  result = events.Present(7, 1, Event::ConfirmationChanged, view);
  assert(result.size() == 1 && result[0].label == "Confirm" && result[0].position == 1);
  view.confirming = false;
  result = events.Present(7, 1, Event::ResetApplied, view);
  assert(result.size() == 2 && result[0].kind == "closed");
  assert(result[1].source == "settings" && result[1].kind == "resumed");
  assert(result[1].title.empty() && result[1].section == view.section);

  // Native child focus can suppress the event that closes a confirmation.
  view.confirming = true;
  events.Present(7, 1, Event::ConfirmationOpened, view);
  view.confirming = false;
  result = events.Present(7, 1, Event::Returned, view);
  assert(result.size() == 2 && result[0].source == "settings_confirmation");
  assert(result[0].kind == "closed");
  assert(result[1].source == "settings" && result[1].kind == "resumed");
  assert(result[1].title.empty() && result[1].section == view.section);
  result = events.Present(7, 1, Event::ValueChanged, view);
  assert(result.size() == 1 && result[0].kind == "focused");
  assert(result[0].label == view.rows[0].label && result[0].title.empty());

  // An unseen confirmation opening is reconciled on return as well.
  view.confirming = true;
  result = events.Present(7, 1, Event::Returned, view);
  assert(result.size() == 1 && result[0].source == "settings_confirmation");
  assert(result[0].kind == "opened" && result[0].body == view.confirmation);
  result = events.Present(7, 1, Event::Returned, view);
  assert(result.size() == 1 && result[0].kind == "resumed");
  assert(result[0].body.empty() && result[0].title.empty());
  view.confirming = false;
  events.Present(7, 1, Event::Returned, view);

  result = events.Closed();
  assert(result.size() == 1 && result[0].source == "settings" && result[0].kind == "closed");
  assert(events.Closed().empty());
  assert(events.Present(7, 1, Event::ValueChanged, view).empty());
  result = events.Present(8, 1, Event::Opened, view);
  assert(result.size() == 1 && result[0].title == view.title);
  view.confirming = true;
  events.Present(8, 1, Event::ConfirmationOpened, view);
  result = events.Closed();
  assert(result.size() == 2 && result[0].source == "settings_confirmation");
  assert(result[1].source == "settings");
}
