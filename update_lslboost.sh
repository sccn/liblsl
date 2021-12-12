# the absolute path to the extracted boost source archive (https://www.boost.org/users/download/)
set -e
set -x
BOOSTPATH=/tmp/boost_1_78_0
TMPPATH=/tmp/lslboost

# copy all needed boost files and rename all mentions of boost to lslboost
mkdir -p $TMPPATH
bcp --unix-lines --boost=$BOOSTPATH --namespace=lslboost --scan `find src -regex ".+\.[ch]p*"` $TMPPATH
# remove superfluous directories:
rm -f $TMPPATH/Jamroot
find $TMPPATH -type d -and \( -name build -o -name test -o -name edg -o -name dmc -o -name msvc?0 -o -name bcc* \) -print0 | xargs -0 rm -rf

rsync -HAXavr --del $TMPPATH/{boost,libs} lslboost

