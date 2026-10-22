<?php
/*
	Markdown to HTML ChangeLog converter
	(C) 2020-2026 - Christophe Calmejane
	Using Parsedown library (http://parsedown.org)

	Requires PHP 8.0 or later.

	Query parameters:
	  - version          (optional) Version to start from. Defaults to the first "## [" entry.
	  - lastKnownVersion (optional) Version to stop at (excluded), or "next" to only return the requested version. Defaults to the end of the file.
	  - fileURL          (optional) Changelog source. Must exactly match an entry of ALLOWED_SOURCES. Defaults to DEFAULT_SOURCE.
*/
require_once "Parsedown.php";

// Changelog returned when no "fileURL" parameter is provided
const DEFAULT_SOURCE = 'CHANGELOG.md';

// Allowed changelog sources (exact match only, no prefix or pattern matching).
// Local entries are resolved relative to this script's directory.
// Remote entries must use HTTPS and point to the final URL (redirects are not followed).
// Edit this list to match your deployment.
const ALLOWED_SOURCES = [
	'CHANGELOG.md',
	// 'https://example.com/path/to/CHANGELOG.md',
];

// Timeout (in seconds) when fetching a remote changelog
const REMOTE_TIMEOUT = 5;

/**
 * Loads the raw content of a changelog source.
 * Returns false if the source cannot be read.
 */
function loadContent(string $source): string|false
{
	if (str_starts_with($source, 'https://'))
	{
		$context = stream_context_create([
			'http' => [
				'timeout' => REMOTE_TIMEOUT,
				'follow_location' => 0,
			],
		]);
		return @file_get_contents($source, false, $context);
	}

	return @file_get_contents(__DIR__ . '/' . $source);
}

/**
 * Extracts the requested section of a Markdown changelog and converts it to HTML.
 * Returns an empty string if the source cannot be read or the version is not found.
 */
function getChangelog(string $source, string $currentVersion, string $lastKnownVersion): string
{
	$content = loadContent($source);
	if ($content === false)
	{
		http_response_code(502);
		return '';
	}

	// Find the start of the requested version (or the first version entry)
	if (strlen($currentVersion) > 0)
	{
		$startPos = strpos($content, "## [" . $currentVersion . "]");
	}
	else
	{
		$startPos = strpos($content, "## [");
	}
	if ($startPos === false)
	{
		return '';
	}

	// Find where to stop
	$endPos = false;
	if (strlen($lastKnownVersion) > 0)
	{
		if ($lastKnownVersion === "next")
		{
			$endPos = strpos($content, "## [", $startPos + 1);
		}
		else
		{
			$endPos = strpos($content, "## [" . $lastKnownVersion . "]", $startPos + 1);
		}
	}
	if ($endPos === false)
	{
		$endPos = strlen($content);
	}

	$changelog = substr($content, $startPos, $endPos - $startPos);

	// Safe mode escapes raw HTML and neutralizes unsafe link schemes (e.g. "javascript:"), since the content may come from a server we do not control
	$parsedown = new Parsedown();
	$parsedown->setSafeMode(true);
	return $parsedown->text($changelog);
}

$version = (string)($_GET["version"] ?? "");
$lastKnownVersion = (string)($_GET["lastKnownVersion"] ?? "");

// Only accept sources explicitly listed in ALLOWED_SOURCES
$source = DEFAULT_SOURCE;
if (isset($_GET["fileURL"]))
{
	if (!in_array($_GET["fileURL"], ALLOWED_SOURCES, true))
	{
		http_response_code(400);
		exit;
	}
	$source = $_GET["fileURL"];
}

header('Content-Type: text/html; charset=utf-8');
echo getChangelog($source, $version, $lastKnownVersion);
