#include "mainmenu.hpp"

MainMenu::MainMenu(spt::Spotify &spotify, lib::settings &settings, QWidget *parent)
	: settings(settings),
	spotify(spotify),
	QMenu(parent)
{
	// Update notifier
	about = addAction(Icon::get("help-about"), QString("Checking for updates..."));
	about->setDisabled(true);

	// Check for updates
	auto mainWindow = dynamic_cast<MainWindow *>(parent);
	if (mainWindow != nullptr)
	{
		auto json = mainWindow
			->getJson("https://api.github.com/repos/kraxarn/spotify-qt/releases/latest");
		auto latest = json.object()["tag_name"].toString();
		auto isLatest = latest.isEmpty() || latest == APP_VERSION;
		if (latest.isEmpty() || isLatest)
			about->setVisible(false);
		else
		{
			about->setDisabled(false);
			about->setText(QString("Update found: %1").arg(latest));
			QAction::connect(about, &QAction::triggered, [this]()
			{
				Utils::openUrl("https://github.com/kraxarn/spotify-qt/releases/latest",
					LinkType::Web, MainWindow::find(this->parentWidget()));
			});
		}
	}

	// Device selection
	deviceMenu = new QMenu("Device", this);
	deviceMenu->setIcon(Icon::get("speaker"));
	QMenu::connect(deviceMenu, &QMenu::aboutToShow, [this]()
	{
		this->refreshDevices();
	});
	QMenu::connect(deviceMenu, &QMenu::triggered, this, &MainMenu::deviceSelected);
	addMenu(deviceMenu);

	// Refresh and settings
	auto openSettings = Utils::createMenuAction("configure",
		"Settings...", QKeySequence::Preferences);
	QAction::connect(openSettings, &QAction::triggered, [this]()
	{
		SettingsDialog dialog(this->settings, MainWindow::find(parentWidget()));
		dialog.exec();
	});
	addAction(openSettings);

	// Debug options if enabled
	if (lib::developer_mode::enabled)
		addMenu(new DeveloperMenu(settings, this));

	// Log out and quit
	addSeparator();
	auto quitAction = Utils::createMenuAction("application-exit", "Quit", QKeySequence::Quit);
	QAction::connect(quitAction, &QAction::triggered, QCoreApplication::quit);
	auto logOutAction = Utils::createMenuAction("im-user-away", "Log out");

	QAction::connect(logOutAction, &QAction::triggered, [this]()
	{
		QMessageBox box(QMessageBox::Icon::Question,
			"Are you sure?",
			"Do you also want to clear your application credentials or only log out?");

		auto clearAll = box.addButton("Clear everything", QMessageBox::ButtonRole::AcceptRole);
		auto logOut = box.addButton("Only log out", QMessageBox::ButtonRole::AcceptRole);
		auto cancel = box.addButton("Cancel", QMessageBox::ButtonRole::RejectRole);
		box.exec();
		auto result = box.clickedButton();

		// Return if we pressed cancel
		if (result == nullptr || result == cancel)
			return;

		// Clear client secret/id if clearAll
		if (result == clearAll)
			this->settings.remove_client();

		// Clear login if cleatAll/logOut
		if (result == clearAll || result == logOut)
			this->settings.remove_tokens();
		this->settings.save();
		QMessageBox::information(MainWindow::find(parentWidget()),
			"Logged out",
			"You are now logged out, the application will now close");

		QCoreApplication::quit();
	});

	addActions({
		logOutAction, quitAction
	});
}

void MainMenu::refreshDevices()
{
	spotify.devices([this](const std::vector<lib::spt::device> &devices)
	{
		// Probably left menu before it loaded
		if (this->deviceMenu == nullptr)
			return;

		// Clear all entries
		for (auto &action : deviceMenu->actions())
			deviceMenu->removeAction(action);

		// Check if empty
		if (devices.empty())
		{
			deviceMenu->addAction("No devices found")->setDisabled(true);
			return;
		}

		// Update devices
		for (auto &device : devices)
		{
			auto action = deviceMenu->addAction(QString::fromStdString(device.name));
			action->setCheckable(true);
			action->setChecked(device.is_active);
			action->setDisabled(device.is_active);
			action->setData(QString::fromStdString(device.id));
		}
	});
}

void MainMenu::deviceSelected(QAction *action)
{
	spotify.setDevice(action->data().toString().toStdString(),
		[this, action](const QString &status)
		{
			if (!status.isEmpty())
			{
				action->setChecked(false);
				auto window = MainWindow::find(this->parentWidget());
				if (window != nullptr)
				{
					window->setStatus(QString("Failed to set device: %1")
						.arg(status), true);
				}
			}
			else
				action->setDisabled(true);
		});
}
