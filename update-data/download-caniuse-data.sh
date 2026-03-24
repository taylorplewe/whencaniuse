#!/usr/bin/bash

source ./paths.sh

output=$(wget -O $CANIUSE_DATA_DEST_FILE $CANIUSE_DATA_URL 2>&1)
if [ $? -ne 0 ]; then
  echo "$(date) - ERROR: could not download caniuse data:" >> $LOG_FILE
  echo "$output" >> $LOG_FILE
else
  echo "$(date) - downloaded caniuse data" >> $LOG_FILE
fi

output=$(wget -O $BCD_DATA_DEST_FILE $BCD_DATA_URL 2>&1)
if [ $? -ne 0 ]; then
  echo "$(date) - ERROR: could not download MDN browser-compat-data:" >> $LOG_FILE
  echo "$output" >> $LOG_FILE
else
  echo "$(date) - downloaded MDN browser-compat-data" >> $LOG_FILE
fi

output=$(wget -O $BROWSER_USAGE_DATA_DEST_FILE $BROWSER_USAGE_DATA_URL 2>&1)
if [ $? -ne 0 ]; then
  echo "$(date) - ERROR: could not download Statcounter browser usage data:" >> $LOG_FILE
  echo "$output" >> $LOG_FILE
else
  echo "$(date) - downloaded Statcounter browser usage data" >> $LOG_FILE
fi

# web-features data.json
echo "$WEB_FEATURES_PACKAGE_INFO_URL"
output=$(wget $(curl "$WEB_FEATURES_PACKAGE_INFO_URL" | jq -r '.dist.tarball'))
if [ $? -ne 0 ]; then
  echo "$(date) - ERROR: could not download web-features tarball from npm registry:" >> $LOG_FILE
  echo "$output" >> $LOG_FILE
else
  tar xzf web-features*tgz*
  mv package/data.json $WEB_FEATURES_DATA_DEST_FILE
  rm -rf package
  rm -rf web-features*tgz*
  echo "$(date) - downloaded web-features npm package data.json" >> $LOG_FILE
fi

# combine into one binary data file
genDataOutput=$(./gen-combined-whencaniuse-data-file)
echo "$(date) - $genDataOutput" >> $LOG_FILE


serverPid=$(pgrep whencaniuse)
if [ $? -ne 0 ]; then
  echo "$(date) - whencaniuse server is not running" >> $LOG_FILE
  exit
fi

# send signal to long running whencaniuse server to update its memory
output=$(kill -SIGUSR1 "$serverPid" 2>&1)
if [ $? -ne 0 ]; then
  echo "$(date) - ERROR: could not send SIGUSR1 signal to whencaniuse server:" >> $LOG_FILE
  echo "$output" >> $LOG_FILE
else
  echo "$(date) - sent SIGUSR1 signal to whencaniuse server (PID: $serverPid)" >> $LOG_FILE
fi
