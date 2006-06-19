# dir2html.sh by Michael Temari 03/03/96
#
#!/bin/sh
if [ $# != 0 ]
then
cd $1
fi
dname=`pwd`
fdname=$2
if [ $dname != / ]
then
  dname=${dname}/
fi
echo "<HTML>"
echo "<TITLE>"
echo Directory of $fdname
echo "</TITLE>"
echo "<H1>"
echo Directory of $fdname
echo "</H1>"
echo "<HR>"
#
ls $dname |
{
while read fname
do
lname=$fdname$fname
echo "<H3>"
echo -n "<A HREF=\""
echo -n $lname
echo -n "\">"
echo -n $fname
echo "</A><BR>"
echo "</H3>"
done
}
echo "<HR>"
echo "<H6>"
echo Directory Generated at `date`
echo "</H6>"
echo "</HTML>"
exit 0
