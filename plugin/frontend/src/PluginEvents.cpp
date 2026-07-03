#include <FrontendEvents.h>

QEvent::Type SourceAddedEvent::type = static_cast<QEvent::Type>(QEvent::registerEventType());
QEvent::Type SourceRemovedEvent::type = static_cast<QEvent::Type>(QEvent::registerEventType());
