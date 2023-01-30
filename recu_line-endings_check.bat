@echo off

set err=0
set exe=line_tool_rel.exe

rem get params
set argC=0
for %%x in (%*) do Set /A argC+=1

if "%~3"=="" (
 echo need 3 args
 set err=1
 goto :errlab
)

set newdir=%~1
set filemask=%~2
set lemode=%~3


rem check lemode (line ending mode)
if NOT "%lemode%" == "D" (
 if NOT "%lemode%" == "U" (
  if NOT "%lemode%" == "M" (
   echo invalid line ending %lemode% ^(not D or U or M^)
   set err=2
   goto :errlab
  )
 )
)

rem echo newdir:%newdir%
rem echo filemask:%filemask%
rem echo lemode:%lemode%



set OLDDIR=%CD%
cd %newdir%
for /R %%f in (%filemask%) do (
echo %%f:
rem echo %OLDDIR%\%exe% -L%lemode% "%%f"
%OLDDIR%\%exe% -L%lemode% "%%f"
)

chdir /d %OLDDIR% &rem restore current directory

EXIT /B 0


:errlab
echo program to list individual lines of given files ^(specified by file mask^) from a directory recursively where line ends with DOS, UNIX or MAC style
echo usage this.bat directory file-mask D/U/M ^(Dos, Unix, Mac line endings^)
echo.
echo exit with error %err%
echo OLDDIR:%CD%
echo param0:"%~0"
if not "%~1"=="" (
 echo param1:"%~1"
 if not "%~2"=="" (
  echo param2:"%~2"
 )
  if not "%~3"=="" (
   echo param3:"%~3"
  )
)

EXIT /B 0
