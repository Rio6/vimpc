/*
   Vimpc
   Copyright (C) 2010 Nathan Sweetman

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   screen.cpp - ncurses window management
   */

#include "screen.hpp"

#include "console.hpp"
#include "mpdclient.hpp"
#include "playlist.hpp"
#include "settings.hpp"

#include <iostream>

using namespace Ui;

Screen::Screen(Mpc::Client & client, Main::Settings const & settings) :
   window_          (Playlist),
   statusWindow_    (NULL),
   commandWindow_   (NULL),
   client_          (client),
   settings_        (settings),
   started_         (false),
   maxRows_         (0),
   maxColumns_      (0)
{
   STATIC_ASSERT((sizeof(mainWindows_)/sizeof(Window *)) == MainWindowCount);

   // \todo for some reason when i run this
   // using gnuscreen the cursor doesn't appear?
   initscr();
   start_color();
   use_default_colors();
   cbreak();
   noecho();

   getmaxyx(stdscr, maxRows_, maxColumns_);

   //Status and Mode window use last 2 rows
   maxRows_ -= 2;

   //Windows
   playlistWindow_        = new Ui::PlaylistWindow(*this, client);
   consoleWindow_         = new Ui::ConsoleWindow (*this);
   libraryWindow_         = new Ui::ConsoleWindow (*this); // \todo
   helpWindow_            = new Ui::ConsoleWindow (*this); // \todo
   statusWindow_          = newwin(0, maxColumns_, maxRows_, 0);

   //Commands must be read through a window that is always visible
   commandWindow_         = statusWindow_;    

   //
   mainWindows_[Playlist] = playlistWindow_;
   mainWindows_[Console]  = consoleWindow_;
   mainWindows_[Library]  = libraryWindow_;
   mainWindows_[Help]     = helpWindow_;
   keypad(commandWindow_, true);
   curs_set(0);

   consoleWindow_->SetAutoScroll(true);
   SetStatusLine("%s", "");

   ENSURE(WindowsAreInitialised() == true);
}

Screen::~Screen()
{
   for (int i = 0; i < MainWindowCount; ++i)
   {
      delete mainWindows_[i];
   }

   delwin(statusWindow_);
   delwin(commandWindow_);
   endwin();
}


void Screen::Start()
{
   REQUIRE(started_ == false);

   if (started_ == false)
   {
      started_ = true;
      SetActiveWindow(settings_.Window());
      wrefresh(statusWindow_);
   }

   ENSURE(started_ == true);
}

ModeWindow * Screen::CreateModeWindow()
{
   return (new ModeWindow(*this));
}

void Screen::SetStatusLine(char const * const fmt, ...)
{
   // \todo needs to be a different colour to the selection
   std::string const BlankLine(maxColumns_, ' ');

   wattron(statusWindow_,   A_REVERSE | A_BOLD);
   mvwprintw(statusWindow_, 0, 0, BlankLine.c_str());
   wmove(statusWindow_,     0, 0);

   va_list args;
   va_start(args, fmt);
   vw_printw(statusWindow_, fmt, args);
   va_end(args);

   wrefresh(statusWindow_);
}


void Screen::Select(Window::Position position, uint32_t count)
{
   mainWindows_[window_]->Select(position, count);
}

void Screen::Scroll(Size size, Direction direction, uint32_t count)
{
   int32_t scrollCount = count;

   if (size == Page)
   {
      scrollCount *= ((MaxRows() + 1) / 2);
   }

   if (direction == Up)
   {
      scrollCount *= -1;
   }

   Scroll(scrollCount);
}

void Screen::ScrollTo(Location location, uint32_t line)
{
   uint32_t scroll[LocationCount] = { 0, 0, 0, 0 };

   scroll[Specific] = line;

   if (window_ == Playlist)
   {
      scroll[Current] = client_.GetCurrentSong() + 1;
      scroll[Bottom]  = client_.TotalNumberOfSongs();
   }

   ScrollTo(scroll[location]);
}

void Screen::Search(std::string const & searchString) const
{
   mainWindows_[window_]->Search(searchString);
}


void Screen::Confirm()
{
   mainWindows_[window_]->Confirm();
}

void Screen::Clear()
{
   consoleWindow_->Clear();

   if (window_ == Console)
   {
      Update();
   }
}

void Screen::Update() const
{
   if ((started_ == true) && (mainWindows_[window_] != NULL))
   {
      Ui::Window & mainWindow = assert_reference(mainWindows_[window_]);

      mainWindow.Erase();

      for (uint32_t i = 0; (i < maxRows_); ++i)
      {
         mainWindow.Print(i);
      }

      mainWindow.Refresh();
      wnoutrefresh(statusWindow_);
      doupdate();
   }
}

void Screen::Redraw()
{
   mainWindows_[window_]->Redraw();
}


uint32_t Screen::MaxRows() const
{
   return maxRows_;
}

uint32_t Screen::MaxColumns() const
{
   return maxColumns_;
}

uint32_t Screen::WaitForInput() const
{
   return wgetch(commandWindow_);
}

void Screen::SetActiveWindow(MainWindow window)
{
   window_ = window;
   Update();
}

Ui::ConsoleWindow & Screen::ConsoleWindow() const
{ 
   return assert_reference(consoleWindow_);
}

Ui::PlaylistWindow & Screen::PlaylistWindow() const
{ 
   return assert_reference(playlistWindow_);
}


Screen::MainWindow Screen::GetWindowFromName(std::string const & windowName)
{
   static WindowTable windowTable;
   static MainWindow  window = Playlist;

   if (windowTable.size() != MainWindowCount)
   {
      // Names for each of the windows
      windowTable["console"]  = Console;
      windowTable["playlist"] = Playlist;
      windowTable["library"]  = Library;
      windowTable["help"]     = Help;
   }

   WindowTable::const_iterator it = windowTable.find(windowName);

   if (it != windowTable.end())
   {
      window = it->second;
   }
   
   ENSURE(windowTable.size() == MainWindowCount);

   return window;
}


void Screen::Scroll(int32_t count)
{
   mainWindows_[window_]->Scroll(count);
}

void Screen::ScrollTo(uint32_t line)
{
   mainWindows_[window_]->ScrollTo(line);
}


bool Screen::WindowsAreInitialised()
{
   bool result = true;

   for (int i = 0; ((i < MainWindowCount) && (result == true)); ++i)
   {
      result = (mainWindows_[i] != NULL);
   }
   
   return result;
}

