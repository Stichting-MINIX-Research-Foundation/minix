%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "usb_driver.h"
#define YY_NO_INPUT
static struct devmand_usb_driver   *current_drv;
static struct devmand_usb_match_id *current_id;

int yylex(void);

void yyerror(char *s)
{
    fprintf(stderr,"parsing error: %s\n",s);
}

int yywrap()
{
    return 1;
}
%}

%union {
       char *string;
}

%start drivers
%token <string>  USB_DRIVER DEV_PREFIX BINARY INTERFACE_CLASS INTERFACE_SUB_CLASS EQUALS DEV_TYPE BLOCK_DEV CHAR_DEV UPSCRIPT DOWNSCRIPT
SEMICOLON BRACKET_OPEN BRACKET_CLOSE STRING ID INTERFACE_PROTOCOL

%%
drivers :
	driver
	{
	}
    | drivers driver
	{
	};

driver :
	USB_DRIVER STRING {current_drv = add_usb_driver($2);}
	BRACKET_OPEN
	usb_driver_statements BRACKET_CLOSE
	{
	};

usb_driver_statements:
	usb_driver_statement
	{
	}
    | usb_driver_statements usb_driver_statement
	{
	};

usb_driver_statement:
	{current_id = add_usb_match_id(current_drv);}
	ID BRACKET_OPEN usb_device_id_statements BRACKET_CLOSE
	{
	}
	| BINARY EQUALS STRING SEMICOLON
	{
		current_drv->binary = $3;
	}
	| DEV_PREFIX EQUALS STRING SEMICOLON
	{
		current_drv->devprefix = $3;
	}
	| DEV_TYPE EQUALS BLOCK_DEV SEMICOLON
	{
		current_drv->dev_type = block_dev;
	}
	| DEV_TYPE EQUALS CHAR_DEV SEMICOLON
	{
		current_drv->dev_type = char_dev;
	}
	| UPSCRIPT EQUALS STRING SEMICOLON
	{
		current_drv->upscript = $3;
	}
	| DOWNSCRIPT EQUALS STRING SEMICOLON
	{
		current_drv->downscript = $3;
	};


usb_device_id_statements:
	usb_device_id_statement
	{
	}
	|usb_device_id_statements usb_device_id_statement
	{
	};


usb_device_id_statement:
	INTERFACE_CLASS EQUALS STRING SEMICOLON
	{
		int res;
		unsigned int num;
		current_id->match_flags |= USB_MATCH_INTERFACE_CLASS;
		res =  sscanf($3, "0x%x", &num);
		if (res != 1) {
			fprintf(stderr, "ERROR");
			exit(1);
		}
		current_id->match_id.bInterfaceClass = num;
	}
	| INTERFACE_SUB_CLASS EQUALS STRING SEMICOLON
	{
		int res;
		unsigned int num;
		current_id->match_flags |= USB_MATCH_INTERFACE_SUBCLASS;
		res =  sscanf($3, "0x%x", &num);
		if (res != 1) {
			fprintf(stderr, "ERROR");
			exit(1);
		}
		current_id->match_id.bInterfaceSubClass = num;

	}
	| INTERFACE_PROTOCOL EQUALS STRING SEMICOLON
	{
		int res;
		unsigned int num;
		current_id->match_flags |= USB_MATCH_INTERFACE_PROTOCOL;
		res =  sscanf($3, "0x%x", &num);
		if (res != 1) {
			fprintf(stderr, "ERROR");
			exit(1);
		}
		current_id->match_id.bInterfaceProtocol = num;

	};
%%
