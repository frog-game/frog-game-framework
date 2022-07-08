@echo off

XCOPY .\git_pre_commit\pre-commit ..\..\.git\hooks /s /e /y
