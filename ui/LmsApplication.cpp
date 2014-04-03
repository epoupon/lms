
#include <Wt/WContainerWidget>
#include <Wt/WBreak>
#include <Wt/WLabel>
#include <Wt/WBootstrapTheme>
#include <Wt/WStackedWidget>
#include <Wt/WMenu>
#include <Wt/WNavigationBar>
#include <Wt/WPopupMenu>
#include <Wt/WPopupMenuItem>

#include "LmsApplication.hpp"


/*
 * The env argument contains information about the new session, and
 * the initial request. It must be passed to the Wt::WApplication
 * constructor so it is typically also an argument for your custom
 * application constructor.
*/
LmsApplication::LmsApplication(const Wt::WEnvironment& env)
: Wt::WApplication(env)
{

	setTheme(new Wt::WBootstrapTheme());

	setTitle("LMS");                               // application title

	Wt::WContainerWidget* container (new Wt::WContainerWidget(root()));

	// Create a navigation bar with a link to a web page.
	Wt::WNavigationBar *navigation = new Wt::WNavigationBar(container);
	navigation->setTitle("LMS");
	navigation->setResponsive(true);

	Wt::WStackedWidget *contentsStack = new Wt::WStackedWidget(container);
	contentsStack->addStyleClass("contents");

	// Setup a Left-aligned menu.
	Wt::WMenu *leftMenu = new Wt::WMenu(contentsStack, container);
	navigation->addMenu(leftMenu);

	_audioWidget = new AudioWidget();
	_videoWidget = new VideoWidget();

	leftMenu->addItem("Audio", _audioWidget);
	leftMenu->addItem("Video", _videoWidget);

	// Setup a Right-aligned menu.
	Wt::WMenu *rightMenu = new Wt::WMenu();
	navigation->addMenu(rightMenu, Wt::AlignRight);

	Wt::WPopupMenu *popup = new Wt::WPopupMenu();
	popup->addItem("Parameters");
	popup->addSeparator();
	popup->addItem("Logout");

	Wt::WMenuItem *item = new Wt::WMenuItem("User");
	item->setMenu(popup);
	rightMenu->addItem(item);

	// Add a Search control.
	_searchEdit = new Wt::WLineEdit();
	_searchEdit->setEmptyText("Search...");

	_searchEdit->keyWentUp().connect(this, &LmsApplication::handleSearch);

	navigation->addSearch(_searchEdit, Wt::AlignLeft);

	container->addWidget(contentsStack);

}


void
LmsApplication::handleSearch(void)
{
	// Check currently selected menu item and search it
	_audioWidget->search( _searchEdit->text().toUTF8() );
}

