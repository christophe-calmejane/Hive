<?php
/*
	News Feed generator
	(C) 2022-2025 - Christophe Calmejane
*/

function addNewsToBuffer($output, $news)
{
	// Add 'title' to output buffer as centered header
	$output .= "<div align='center'><h2>" . $news["title"] . "</h2></div>";

	// Add 'content' to output buffer as paragraph
	$output .= "<p>" . $news["content"] . "</p>";

	// Leave a blank line between news items
	$output .= "<br>";

	return $output;
}

// Check for special parameter to display the current server time as a timestamp
if(isset($_GET["getTime"]))
{
	echo time();
	return;
}

$lastCheckTime = "";
if(!isset($_GET["lastCheckTime"]))
{
	echo "{\"error\":\"No lastCheckTime specified\"}";
	return;
}
$lastCheckTime = $_GET["lastCheckTime"];

$buildNumber = "";
if(!isset($_GET["buildNumber"]))
{
	echo "{\"error\":\"No buildNumber specified\"}";
	return;
}
$buildNumber = $_GET["buildNumber"];

$fileURL = "news.json";
if(isset($_GET["fileURL"]))
{
	$fileURL = $_GET["fileURL"];
}

// Open json file
$json = file_get_contents($fileURL);

// Parse json
$json_a = json_decode($json, true);

// Create an output buffer
$output = "";

// Iterate through news
foreach ($json_a as $news)
{
	// Check if news 'date' is newer than lastCheckTime
	if ($news['date'] > $lastCheckTime)
	{
		// Check if buildNumber is greater or equal to news 'start_version' and less or equal to news 'end_version'
		if ($buildNumber >= $news["start_version"] && $buildNumber <= $news["end_version"])
		{
			// Add news to output buffer
			$output = addNewsToBuffer($output, $news);
		}
	}
}

// Return output (escape quotes) as json object along with the server time as a timestamp
echo "{\"news\":\"" . str_replace("\"", "\\\"", $output) . "\",\"serverTimestamp\":" . time() . "}";

?>
