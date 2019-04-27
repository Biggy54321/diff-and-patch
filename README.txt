PROJECT TITLE: diff-and-patch
NAME: Bhaskar Subhash Pardeshi
MIS ID: 111703041

USAGE OF THE PROGRAMS --

#########################################################################
#		diff ->	    ./project1 [options] Fromfile Tofile			   
														         
#		patch -> ./project2 [options] FileToBePatched PatchFile	   
#########################################################################

PROJECT DESCRIPTION --

## diff ##
	The diff program shows the differences between the two files line by line. The program uses Myer's
diff algorithm. It takes two arguments - FROM-file and TO-file. There are basic 2 operation using which
we can convert FROM-file to TO-file that are ADD or DELETE. The operation CHANGE is a deletion followed
by addition. In default diff the add, delete change are represented by letters 'a', 'd' and 'c'. The lines
followed by '<' belong to the FROM-file and the lines followed by '>' belong to the TO-file. (Addition
always takes place from second file and deletion from the first file.)
     The options provided by diff are - 
     		1. i -- This option is used to ignore the uppercase and lowercase letters in the while comparing
     	              two lines.
     		2. b -- This option is used to ignore the change in the amount of existing white spaces in two
     	              lines.
		3. w -- This option completely ignores the white spaces in the lines being compared.
		4. t -- This option expands all the tabs in the lines being compared to spaces.
		5. y -- This option prints the difference between two files in side - by - side format.
		        Here add, delete, change are represented by '>', '<', '|' and common lines by ' '.
		6. c -- This option prints the difference between two files in context format. This is the
		        diff that is used in patching two files. Here add, delete , change are represented by
		        '+', '-', '!' and common lines by ' '.
		7. r -- This option is used to recursively compare the subdirectories, while diff is working
		        on directories.
	The diff for directories print the differences in sorted manner with key as their name. The
differences are printed as - 'Only in DIR_PATH: FILE_NAME/DIR_NAME' and common files undergo diff and the
diff is printed as well the common directories are printed as - 'Common subdirectories - DIR_PATH1/DIR_NAME1
DIR_PATH2/DIR_NAME2'. In case of (recursion) '-r' the Common subdirectoried part is not printed but they 
are again passed to print their difference to the program. This continue till we have no common
subdirectories.

## patch ##
	The patch program merges the differences created by the diff program on the first file or on the
second file. The patch program accepts either default diff or context diff option for patching the two 
files. It takes to arguments - the file to be patched and the patch file.
	The patch program works on either default diff or context diff. The set of lines printed in the
patch file which belong to the FROM-file are are searched by the patch program in the file to be patched.
If the lines are found the the corresponding lines printed in the path file which belong to the TO-file
are  written in the file to be patched. If the lines are not found then the patch fails for that set of
lines. The rejected lines are stored in a .rej file in context format. If the lines to be searched are
found at a location other than the location specifed by the patch, the patch produces an offset and then
writes the lines from the second file. Also if patch does not find the lines at all, then patch ignores
the top and bottom lines of the set and then searches again. If found the patch writes the lines from
the second file ignoring the top and bottom again. The tolerance is about 2 lines from top and 2 lines
from bottom. This is called as fuzz.
	The options provided by patch are-
		R -- searches the lines mentioned in the second file and applies the lines mentioned in the
		first file i.e. it converts the TO-file to FROM-file.
