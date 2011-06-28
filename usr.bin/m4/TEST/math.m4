dnl $NetBSD: math.m4,v 1.2 2005/10/06 17:38:09 drochner Exp $
dnl FreeBSD: /repoman/r/ncvs/src/usr.bin/m4/TEST/math.m4,v 1.1 2004/05/01 03:27:05 smkelly Exp
dnl A regression test for m4 C operators		(ksb,petef)
dnl If you think you have a short-circuiting m4, run us m4 -DSHORCIRCUIT=yes
dnl
dnl first level of precedence
ifelse(eval(-7),-7,,`failed -
')dnl
ifelse(eval(- -2),2,,`failed -
')dnl
ifelse(eval(!0),1,,`failed !
')dnl
ifelse(eval(!7),0,,`failed !
')dnl
ifelse(eval(~-1),0,,`failed ~
')dnl
dnl next level of precedence
ifelse(eval(3*5),15,,`failed *
')dnl
ifelse(eval(3*0),0,,`failed *
')dnl
ifelse(eval(11/2),5,,`failed /
')dnl
ifelse(eval(1/700),0,,`failed /
')dnl
ifelse(eval(10%5),0,,`failed %
')dnl
ifelse(eval(2%5),2,,`failed %
')dnl
ifelse(eval(2%-1),0,,`failed %
')dnl
dnl next level of precedence
ifelse(eval(2+2),4,,`failed +
')dnl
ifelse(eval(2+-2),0,,`failed +
')dnl
ifelse(eval(2- -2),4,,`failed -
')dnl
ifelse(eval(2-2),0,,`failed -
')dnl
dnl next level of precedence
ifelse(eval(1<<4),16,,`failed <<
')dnl
ifelse(eval(16>>4),1,,`failed >>
')dnl
dnl next level of precedence
ifelse(eval(4<4),0,,`failed <
')dnl
ifelse(eval(4<5),1,,`failed <
')dnl
ifelse(eval(4<3),0,,`failed <
')dnl
ifelse(eval(4>4),0,,`failed >
')dnl
ifelse(eval(4>5),0,,`failed >
')dnl
ifelse(eval(4>3),1,,`failed >
')dnl
ifelse(eval(4<=4),1,,`failed <=
')dnl
ifelse(eval(4<=5),1,,`failed <=
')dnl
ifelse(eval(4<=3),0,,`failed <=
')dnl
ifelse(eval(4>=4),1,,`failed >=
')dnl
ifelse(eval(4>=5),0,,`failed >=
')dnl
ifelse(eval(4>=3),1,,`failed >=
')dnl
dnl next level of precedence
ifelse(eval(1==1),1,,`failed ==
')dnl
ifelse(eval(1==-1),0,,`failed ==
')dnl
ifelse(eval(1!=1),0,,`failed !=
')dnl
ifelse(eval(1!=2),1,,`failed !=
')dnl
dnl next level of precedence
ifelse(eval(3&5),1,,`failed &
')dnl
ifelse(eval(8&7),0,,`failed &
')dnl
dnl next level of precedence
ifelse(eval(1^1),0,,`failed ^
')dnl
ifelse(eval(21^5),16,,`failed ^
')dnl
dnl next level of precedence
ifelse(eval(1|1),1,,`failed |
')dnl
ifelse(eval(21|5),21,,`failed |
')dnl
ifelse(eval(100|1),101,,`failed |
')dnl
dnl next level of precedence
ifelse(eval(1&&1),1,,`failed &&
')dnl
ifelse(eval(0&&1),0,,`failed &&
')dnl
ifelse(eval(1&&0),0,,`failed &&
')dnl
ifelse(SHORTCIRCUIT,`yes',`ifelse(eval(0&&10/0),0,,`failed && shortcircuit
')')dnl
dnl next level of precedence
ifelse(eval(1||1),1,,`failed ||
')dnl
ifelse(eval(1||0),1,,`failed ||
')dnl
ifelse(eval(0||0),0,,`failed ||
')dnl
ifelse(SHORTCIRCUIT,`yes',`ifelse(eval(1||10/0),1,,`failed || shortcircuit
')')dnl
dnl next level of precedence
ifelse(eval(0 ? 2 : 5),5,,`failed ?:
')dnl
ifelse(eval(1 ? 2 : 5),2,,`failed ?:
')dnl
ifelse(SHORTCIRCUIT,`yes',`ifelse(eval(0 ? 10/0 : 7),7,,`failed ?: shortcircuit
')')dnl
ifelse(SHORTCIRCUIT,`yes',`ifelse(eval(1 ? 7 : 10/0),7,,`failed ?: shortcircuit
')')dnl
dnl operator precedence
ifelse(eval(!0*-2),-2,,`precedence wrong, ! *
')dnl
ifelse(eval(~8/~2),3,,`precedence wrong ~ /
')dnl
ifelse(eval(~-20%7),5,,`precedence wrong ~ %
')dnl
ifelse(eval(3*2+100),106,,`precedence wrong * +
')dnl
ifelse(eval(3+2*100),203,,`precedence wrong + *
')dnl
ifelse(eval(2%5-6/3),0,,`precedence wrong % -
')dnl
ifelse(eval(2/5-5%3),-2,,`precedence wrong / -
')dnl
ifelse(eval(2+5%5+1),3,,`precedence wrong % +
')dnl
ifelse(eval(7+9<<1),32,,`precedence wrong + <<
')dnl
ifelse(eval(35-3>>2),8,,`precedence wrong - >>
')dnl
ifelse(eval(9<10<<5),1,,`precedence wrong << <
')dnl
ifelse(eval(9>10<<5),0,,`precedence wrong << >
')dnl
ifelse(eval(32>>2<32),1,,`precedence wrong >> <
')dnl
ifelse(eval(9<=10<<5),1,,`precedence wrong << <
')dnl
ifelse(eval(5<<1<=20>>1),1,,`precedence wrong << <=
')dnl
ifelse(eval(5<<1>=20>>1),1,,`precedence wrong << >=
')dnl
ifelse(eval(0<7==5>=5),1,,`precedence wrong < ==
')dnl
ifelse(eval(0<7!=5>=5),0,,`precedence wrong < !=
')dnl
ifelse(eval(0>7==5>=5),0,,`precedence wrong > ==
')dnl
ifelse(eval(0>7!=5>=5),1,,`precedence wrong > !=
')dnl
ifelse(eval(1&7==7),1,,`precedence wrong & ==
')dnl
ifelse(eval(0&7!=6),0,,`precedence wrong & !=
')dnl
ifelse(eval(9&1|5),5,,`precedence wrong & |
')dnl
ifelse(eval(9&1^5),4,,`precedence wrong & ^
')dnl
ifelse(eval(9^1|5),13,,`precedence wrong ^ |
')dnl
ifelse(eval(5|0&&1),1,,`precedence wrong | &&
')dnl
ifelse(eval(5&&0||0&&5||5),1,,`precedence wrong && ||
')dnl
ifelse(eval(0 || 1 ? 0 : 1),0,,`precedence wrong || ?:
')dnl
ifelse(eval(5&&(0||0)&&(5||5)),0,,`precedence wrong || parens
')dnl
