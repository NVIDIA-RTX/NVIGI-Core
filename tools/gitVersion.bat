@echo off
setlocal enabledelayedexpansion

git log -1 --pretty=format:"SHA:%%x20%%h" > tmpFile 
set /p cl=<tmpFile
del tmpFile
git log -1 --pretty=format:"%%h" > tmpFile 
set /p cls=<tmpFile
del tmpFile
git branch --show-current > tmpFile
set /p br=<tmpFile
del tmpFile
git log -1 --pretty=format:"%%H" > tmpFile
set /p sha=<tmpFile
del tmpFile

set branch=branch %br% - sha %sha%

del _gitVersion.h /f
echo #ifndef _GIT_VERSION_HEADER >> _gitVersion.h
echo #define _GIT_VERSION_HEADER >> _gitVersion.h
echo //! Auto generated - start >> _gitVersion.h
echo #define GIT_LAST_COMMIT "%cl%" >> _gitVersion.h
echo #define GIT_LAST_COMMIT_SHORT "%cls%" >> _gitVersion.h
echo #define GIT_BRANCH_AND_LAST_COMMIT "%branch%" >> _gitVersion.h
echo //! Auto generated - end >> _gitVersion.h
echo #endif >> _gitVersion.h

IF EXIST "gitVersion.h" (
    fc gitVersion.h _gitVersion.h > nul && goto no_change
    del gitVersion.h /f
)
move _gitVersion.h gitVersion.h
exit 0

:no_change

echo "### Git sha did not change - nothing to do"