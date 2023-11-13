#!/bin/bash

if [ "$#" -ne 6 ]; then
    echo "Usage: $0 <base_url> <user> <artist_count> <album_count> <song_count> <batch_size>"
    exit 1
fi

# make any command failure exit
set -e

read -r -s -p "Enter password: " user_password
echo

# Retrieve the argument from the command line
base_url="$1"
user="$2"
artist_count="$3"
album_count="$4"
song_count="$5"
batch_size="$6"

start_time=$(date +%s.%3N)

# artists
for ((i = 0; i < artist_count; i += $batch_size)); do
	wget -q -O - "$base_url/rest/search3.view?u=$user&p=$user_password&v=1.13.0&c=benchmark&f=json&query=&artistCount=$batch_size&artistOffset=$i&albumCount=0&songCount=0" > /dev/null
done

# albums
for ((i = 0; i < album_count; i += $batch_size)); do
	wget -q -O - "$base_url/rest/search3.view?u=$user&p=$user_password&v=1.13.0&c=benchmark&f=json&query=&artistCount=0&albumCount=$batch_size&albumOffset=$i&songCount=0" > /dev/null
done

# songs
for ((i = 0; i < song_count; i += $batch_size)); do
	wget -q -O - "$base_url/rest/search3.view?u=$user&p=$user_password&v=1.13.0&c=benchmark&f=json&query=&artistCount=0&albumCount=0&songCount=$batch_size&songOffset=$i" > /dev/null
done

end_time=$(date +%s.%3N)

elapsed_time=$(echo "$end_time - $start_time" | bc)

echo "Fetch time: $elapsed_time seconds"