<?php
/*
	News Feed generator
	(C) 2022-2026 - Christophe Calmejane

	Requires PHP 8.0 or later.

	Query parameters:
	  - getTime        (optional) If present, only returns the current server time as a Unix timestamp.
	  - lastCheckTime  (required) Unix timestamp. Only news with a more recent 'date' are returned.
	  - buildNumber    (required) Numeric build number of the calling application.
	  - os             (optional) OS of the calling application. News with an 'os' field are only returned if it matches. News without an 'os' field are returned for all OSes.
	  - fileURL        (optional) News source. Must exactly match an entry of ALLOWED_SOURCES. Defaults to DEFAULT_SOURCE.

	Response: {"news":"<html>","serverTimestamp":<unix timestamp>}
	      or: {"error":"<message>"}

	Note: news 'title' and 'content' are inserted as raw HTML, so only list trusted sources.
*/

// News file returned when no "fileURL" parameter is provided
const DEFAULT_SOURCE = 'news.json';

// Allowed news sources (exact match only, no prefix or pattern matching).
// Local entries are resolved relative to this script's directory.
// Remote entries must use HTTPS and point to the final URL (redirects are not followed).
// Edit this list to match your deployment.
const ALLOWED_SOURCES = [
	'news.json',
	// 'https://example.com/path/to/news.json',
];

// Timeout (in seconds) when fetching a remote news file
const REMOTE_TIMEOUT = 5;

// JSON encoding flags used for all responses
const JSON_FLAGS = JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE;

/**
 * Sends a JSON error response and stops the script.
 */
function sendError(string $message, int $httpCode = 400): never
{
	http_response_code($httpCode);
	echo json_encode(["error" => $message], JSON_FLAGS);
	exit;
}

/**
 * Loads the raw content of a news source.
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
 * Appends a news item to the HTML output buffer.
 */
function addNewsToBuffer(string $output, array $news): string
{
	// Add 'title' to output buffer as centered header
	$output .= "<div align='center'><h2>" . ($news["title"] ?? "") . "</h2></div>";
	// Add 'content' to output buffer as paragraph
	$output .= "<p>" . ($news["content"] ?? "") . "</p>";
	// Leave a blank line between news items
	$output .= "<br>";
	return $output;
}

// Special parameter: only return the current server time as a timestamp
if (isset($_GET["getTime"]))
{
	echo time();
	exit;
}

// Validate required numeric parameters
if (!isset($_GET["lastCheckTime"]))
{
	sendError("No lastCheckTime specified");
}
if (!is_numeric($_GET["lastCheckTime"]))
{
	sendError("Invalid lastCheckTime");
}
$lastCheckTime = (float)$_GET["lastCheckTime"];

if (!isset($_GET["buildNumber"]))
{
	sendError("No buildNumber specified");
}
if (!is_numeric($_GET["buildNumber"]))
{
	sendError("Invalid buildNumber");
}
$buildNumber = (float)$_GET["buildNumber"];

$os = (string)($_GET["os"] ?? "");

// Only accept sources explicitly listed in ALLOWED_SOURCES
$source = DEFAULT_SOURCE;
if (isset($_GET["fileURL"]))
{
	if (!in_array($_GET["fileURL"], ALLOWED_SOURCES, true))
	{
		sendError("Invalid fileURL");
	}
	$source = $_GET["fileURL"];
}

// Load and parse the news file
$json = loadContent($source);
if ($json === false)
{
	sendError("Unable to load news", 502);
}
$newsList = json_decode($json, true);
if (!is_array($newsList))
{
	sendError("Invalid news file", 502);
}

// Build the HTML output
$output = "";
foreach ($newsList as $news)
{
	// Skip malformed entries
	if (!is_array($news) || !isset($news["date"], $news["start_version"], $news["end_version"]))
	{
		continue;
	}

	// Only keep news newer than lastCheckTime
	if ((float)$news["date"] <= $lastCheckTime)
	{
		continue;
	}

	// Only keep news whose version range includes buildNumber
	if ($buildNumber < (float)$news["start_version"] || $buildNumber > (float)$news["end_version"])
	{
		continue;
	}

	// OS compatibility: if the news has an 'os' field, it must match the provided OS parameter.
	// News without an 'os' field are compatible with all OSes.
	if ($os !== "" && isset($news["os"]) && $news["os"] !== $os)
	{
		continue;
	}

	$output = addNewsToBuffer($output, $news);
}

// Return output as a JSON object along with the server time as a timestamp
echo json_encode(["news" => $output, "serverTimestamp" => time()], JSON_FLAGS);
