/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

// Jennygram: change the own account password on the internal server.
// The request rides on help.saveAppLog with one inputAppEvent of type
// "jg.change_password" and data jsonObject { "old": ..., "new": ... }.
void JennygramChangePasswordBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller);
