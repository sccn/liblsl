# the absolute path to the extracted boost source archive (https://www.boost.org/users/download/)
BOOSTPATH=/tmp/boost_1_68_0

# Remove the old boost sources
rm -rf lslboost/{boost,libs}
# copy all needed boost files and rename all mentions of boost to lslboost
bcp --unix-lines --boost=$BOOSTPATH --namespace=lslboost --scan `find src -regex ".+\.[ch]p*"` lslboost/
# remove superfluous directories:
rm lslboost/Jamroot
find lslboost -type d -and \( -name build -o -name test -o -name edg -o -name dmc -o -name msvc70 -o -name msvc60 -o -name bcc* \) -print0 | xargs -0 rm -rf

# apply patches to boost
# remove minimum wait time
patch -p3 < lslboost/boost-thread-windows.patch
