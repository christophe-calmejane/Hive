<?php
/*
	Markdown to HTML ChangeLog converter
	(C) 2020-2025 - Christophe Calmejane

	Using Parsedown library (http://parsedown.org)
*/
require_once "Parsedown.php";

function getChangelog($fileName, $currentVersion, $lastKnownVersion)
{
	$content = file_get_contents($fileName);

	if (strlen($currentVersion) > 0)
	{
		$startPos = strpos($content, "## [" . $currentVersion . "]");
	}
	else
	{
		$startPos = strpos($content, "## [");
	}
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

	$parsedown = new Parsedown();
	return $parsedown->text($changelog);
}

$version = "";
if(isset($_GET["version"]))
{
	$version = $_GET["version"];
}

$lastKnownVersion = "";
if(isset($_GET["lastKnownVersion"]))
{
	$lastKnownVersion = $_GET["lastKnownVersion"];
}

$fileURL = "CHANGELOG.md";
if(isset($_GET["fileURL"]))
{
	$fileURL = $_GET["fileURL"];
}

echo getChangelog($fileURL, $version, $lastKnownVersion)

?>