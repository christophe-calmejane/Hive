#!/usr/bin/env bash

function extend_lcf_fnc_init()
{
	knownOptions+=("newsfeed_url")
	params["newsfeed_url"]="https://localhost/news.php"
}
