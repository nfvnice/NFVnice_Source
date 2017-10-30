git status
echo "========= START Listing Differences ========================"
git status | awk '$1 == "modified:" { print ($2)}' | git add $2
echo "========= END Listing Differences ========================"
