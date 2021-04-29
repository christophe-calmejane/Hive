/*
* Copyright (C) 2017-2021, Emilien Vallot, Christophe Calmejane and other contributors

* This file is part of Hive.

* Hive is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* Hive is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.

* You should have received a copy of the GNU Lesser General Public License
* along with Hive.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sparkleHelper.hpp"

#import <Sparkle/Sparkle.h>
#import <Foundation/Foundation.h>

/** std::string to NSString conversion */
static inline NSString* getNSString(std::string const& cString)
{
	return [NSString stringWithCString:cString.c_str() encoding:NSUTF8StringEncoding];
}

/** NSString to std::string conversion */
static inline std::string getStdString(NSString* nsString)
{
	return std::string{ [nsString UTF8String] };
}

@interface SparkleDelegate<SUUpdaterDelegate> : NSObject
+ (SparkleDelegate*)getInstance;
@end

@implementation SparkleDelegate

+ (SparkleDelegate*)getInstance {
	static SparkleDelegate* s_Instance = nil;

	@synchronized(self)
	{
		if (s_Instance == nil)
		{
			s_Instance = [[self alloc] init];
		}
	}

	return s_Instance;
}

- (BOOL)updaterMayCheckForUpdates:(SUUpdater*)updater {
	return TRUE;
}

- (BOOL)updaterShouldPromptForPermissionToCheckForUpdates:(SUUpdater*)updater {
	return TRUE;
}

- (void)updater:(SUUpdater*)updater didFindValidUpdate:(SUAppcastItem*)item {
	auto const& handler = Sparkle::getInstance().getLogHandler();
	if (handler)
	{
		handler("A new update has been found", Sparkle::LogLevel::Info);
	}
}

- (void)updaterDidNotFindUpdate:(SUUpdater*)updater {
}

- (BOOL)updaterShouldShowUpdateAlertForScheduledUpdate:(SUUpdater*)updater forItem:(SUAppcastItem*)item {
	return TRUE;
}

- (void)updater:(SUUpdater*)updater willDownloadUpdate:(SUAppcastItem*)item withRequest:(NSMutableURLRequest*)request {
}

- (void)updater:(SUUpdater*)updater didDownloadUpdate:(SUAppcastItem*)item {
}

- (void)updater:(SUUpdater*)updater failedToDownloadUpdate:(SUAppcastItem*)item error:(NSError*)error {
}

- (void)updaterWillShowModalAlert:(SUUpdater*)updater {
}

- (void)updater:(SUUpdater*)updater willExtractUpdate:(SUAppcastItem*)item {
}

- (void)updater:(SUUpdater*)updater didExtractUpdate:(SUAppcastItem*)item {
}

- (void)updater:(SUUpdater*)updater willInstallUpdate:(SUAppcastItem*)item {
}

- (void)updater:(SUUpdater*)updater didAbortWithError:(NSError*)error {
	auto const& handler = Sparkle::getInstance().getLogHandler();
	if (handler)
	{
		handler(std::string{ "Automatic update failed: " } + getStdString([error description]), Sparkle::LogLevel::Warn);
	}
}

- (void)updater:(SUUpdater*)updater willInstallUpdateOnQuit:(SUAppcastItem*)item immediateInstallationInvocation:(NSInvocation*)invocation {
}

@end

void Sparkle::init(std::string const& /*internalNumber*/, std::string const& signature) noexcept
{
	auto* const updater = [SUUpdater sharedUpdater];

	updater.delegate = [SparkleDelegate getInstance];

	// Get current Check For Updates value
	_checkForUpdates = updater.automaticallyChecksForUpdates;

	_initialized = true;
}

void Sparkle::start() noexcept
{
	if (!_initialized)
	{
		return;
	}

	// Start updater
	auto* const updater = [SUUpdater sharedUpdater];
	[updater resetUpdateCycle];
	[updater checkForUpdatesInBackground];

	_started = true;
}

void Sparkle::setAutomaticCheckForUpdates(bool const checkForUpdates) noexcept
{
	if (!_initialized)
	{
		return;
	}

	// Set Automatic Check For Updates
	auto* const updater = [SUUpdater sharedUpdater];
	updater.automaticallyChecksForUpdates = checkForUpdates;

	// If switching to CheckForUpdates, check right now
	if (checkForUpdates && _started)
	{
		[updater resetUpdateCycle];
		[updater checkForUpdatesInBackground];
	}

	_checkForUpdates = checkForUpdates;
}

void Sparkle::setAppcastUrl(std::string const& appcastUrl) noexcept
{
	if (!_initialized)
	{
		return;
	}

	// Set Appcast URL
	auto* const updater = [SUUpdater sharedUpdater];
	updater.feedURL = [NSURL URLWithString:getNSString(appcastUrl)];

	if (appcastUrl != _appcastUrl && _started && _checkForUpdates)
	{
		[updater resetUpdateCycle];
		[updater checkForUpdatesInBackground];
	}

	_appcastUrl = appcastUrl;
}

void Sparkle::manualCheckForUpdate() noexcept
{
	if (!_initialized)
	{
		return;
	}

	auto* const updater = [SUUpdater sharedUpdater];
	[updater checkForUpdatesInBackground];
}

Sparkle::~Sparkle() noexcept
{
	// Nothing to do
}
