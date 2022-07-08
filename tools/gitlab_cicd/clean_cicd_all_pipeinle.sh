#!/bin/bash
set -e

YOUR_GITLAB_SERVER="http://10.17.17.21"
TOKEN="W58Ay4_-zr2B7P614ePL"
YOUR_PROJECT_ID=3
# How many to delete from the oldest.
PER_PAGE=100

for PIPELINE in $(curl --header "PRIVATE-TOKEN: $TOKEN" "$YOUR_GITLAB_SERVER/api/v4/projects/$YOUR_PROJECT_ID/pipelines?per_page=$PER_PAGE&sort=asc" | jq '.[].id') ; do
    echo "Deleting pipeline $PIPELINE"
    curl --header "PRIVATE-TOKEN: $TOKEN" --request "DELETE" "$YOUR_GITLAB_SERVER/api/v4/projects/$YOUR_PROJECT_ID/pipelines/$PIPELINE"
done
