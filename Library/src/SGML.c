/*									 SGML.c
**	GENERAL SGML PARSER CODE
**
**	(c) COPYRIGHT MIT 1995.
**	Please first read the full copyright statement in the file COPYRIGH.
**	@(#) $Id$
**
**	This module implements an HTStream object. To parse an
**	SGML file, create this object which is a parser. The object
**	is (currently) created by being passed a DTD structure,
**	and a target HTStructured oject at which to throw the parsed stuff.
**	
**	 6 Feb 93  	Binary seraches used. Intreface modified.
**	 8 Jul 94  FM	Insulate free() from _free structure element.
**	Nov 1996   msa	Strip down the parser to minimal HTML tokenizer,
**			Stop allocating space for the attribute values,
**			use pointers to the string chunk instead.
*/

/* Library include files */
#include "sysdep.h"
#include "HTUtils.h"
#include "HTString.h"
#include "HTChunk.h"
#include "SGML.h"

#define INVALID (-1)

/*	The State (context) of the parser
**
**	This is passed with each call to make the parser reentrant
**
*/
typedef enum _sgml_state
    {
	S_text, S_literal, S_tag, S_tag_gap, 
	S_attr, S_attr_gap, S_equals, S_value, S_after_open,
	S_nl, S_nl_tago,
	S_ero, S_cro,
#ifdef ISO_2022_JP
	S_esc, S_dollar, S_paren, S_nonascii_text,
#endif
	S_squoted, S_dquoted, S_end, S_entity, S_junk_tag,
	S_md, S_md_sqs, S_md_dqs, S_com_1, S_com, S_com_2, S_com_2a
    } sgml_state;


/*	Internal Context Data Structure
**	-------------------------------
*/
struct _HTStream
    {
	const HTStreamClass *isa;	/* inherited from HTStream */
	const SGML_dtd *dtd;
	HTStructuredClass *actions;	/* target class  */
	HTStructured *target;		/* target object */

	HTTag *current_tag;
	int current_attribute_number;
	SGMLContent contents;		/* current content mode */
	HTChunk *string;
	int token;			/* ptr into string buffer */
	sgml_state state;
	BOOL present[MAX_ATTRIBUTES];	/* Flags: attribute is present? */
	int value[MAX_ATTRIBUTES];	/* Offset pointers to the string */
    };


#define PUTC(ch) ((*context->actions->put_character)(context->target, ch))
#define PUTB(b,l) ((*context->actions->put_block)(context->target, b, l))

#define TRACE1(f,a) \
	do {if (SGML_TRACE) HTTrace((f),(a)); } while(0)
#define TRACE2(f,a,b) \
	do {if (SGML_TRACE) HTTrace((f),(a),(b)); } while(0)

/*	Find Attribute Number
**	---------------------
*/
PRIVATE int SGMLFindAttribute  (HTTag* tag, const char * s)
    {
	attr* attributes = tag->attributes;

	int high, low, i, diff;		/* Binary search for attribute name */
	for(low=0, high=tag->number_of_attributes;
	    high > low ;
	    diff < 0 ? (low = i+1) : (high = i) )
	    {
		i = (low + (high-low)/2);
		diff = strcasecomp(attributes[i].name, s);
		if (diff==0)
			return i;	/* success: found it */
	    }
	return -1;
    }


/*	Handle Attribute
**	----------------
*/
/* PUBLIC const char * SGML_default = "";   ?? */

PRIVATE void handle_attribute_name (HTStream * context, const char * s)
    {
	HTTag * tag = context->current_tag;

	/* Note: if tag==NULL, we are skipping unknown tag... */
	if (tag)
	    {
		int i = SGMLFindAttribute(tag, s);
		if (i >= 0)
		    {
			context->current_attribute_number = i;
			context->present[i] = YES;
			return;
		    }
		TRACE2("Unknown attribute %s for tag %s\n",
		       s, context->current_tag->name);
	    }
	context->current_attribute_number = INVALID;	/* Invalid */
    }


/*	Handle attribute value
**	----------------------
*/
PRIVATE void handle_attribute_value (HTStream * context)
    {
	/* Deal with attributes only if tag is known,
	   ignore silently otherwise */

	if (context->current_tag)
	    {
		if (context->current_attribute_number != INVALID)
			context->value[context->current_attribute_number] =
				context->token;
		else
			TRACE1("Attribute value %s ignored\n",
			       context->string->data + context->token);

	    }
	context->current_attribute_number = INVALID; /* can't have two assignments! */
    }

/*	Handle entity
**	-------------
**
** On entry,
**	s	contains the entity name zero terminated
*/
PRIVATE void handle_entity (HTStream * context)
    {
	const char ** entities = context->dtd->entity_names;
	const char *s = context->string->data;

	int high, low, i, diff;
	for(low=0, high = context->dtd->number_of_entities;
	    high > low ;
	    diff < 0 ? (low = i+1) : (high = i))
	    {
		i = (low + (high-low)/2);
		diff = strcmp(entities[i], s);	/* Case sensitive! */
		if (diff==0)
		    {	/* success: found it */
			(*context->actions->put_entity)(context->target, i);
			return;
		    }
	    }
	/* If entity string not found, display as text */
	TRACE1("Unknown entity %s\n", s);
	PUTC('&');
	    {
		const char *p;
		for (p=s; *p; p++)
			PUTC(*p);
	    }
    }

/*	End element
**	-----------
*/
PRIVATE void end_element (HTStream * context, HTTag *tag)
    {
	TRACE1("End   </%s>\n", tag->name);
	(*context->actions->end_element)
		(context->target, tag - context->dtd->tags);
    }

/*	Start an element
**	----------------
*/
PRIVATE void start_element (HTStream * context)
    {
	int i;
	char *value[MAX_ATTRIBUTES];
	HTTag *tag = context->current_tag;

	TRACE1("Start <%s>\n", tag->name);
	context->contents = tag->contents;

	/*
	** Build the actual pointers to the value strings stored in the
	** chunk buffer. (Must use offsets while collecting the values,
	** because the string chunk may get resized during the collection
	** and potentially relocated).
	*/
	for (i = 0; i < MAX_ATTRIBUTES; ++i)
		value[i] = context->value[i] < 0 ? NULL :
			context->string->data + context->value[i];
	(*context->actions->start_element)
		(context->target,
		 tag - context->dtd->tags,
		 context->present,
		 (const char**)value);  /* coerce type for think c */
    }


/*		Find Tag in DTD tag list
**		------------------------
**
** On entry,
**	dtd	points to dtd structire including valid tag list
**	string	points to name of tag in question
**
** On exit,
**	returns:
**		NULL		tag not found
**		else		address of tag structure in dtd
*/
PRIVATE HTTag * SGMLFindTag (const SGML_dtd* dtd, const char * string)
    {
	int high, low, i, diff;
	for(low=0, high=dtd->number_of_tags;
	    high > low ;
	    diff < 0 ? (low = i+1) : (high = i))
	    {  /* Binary serach */
		i = (low + (high-low)/2);
		diff = strcasecomp(dtd->tags[i].name, string);	/* Case insensitive */
		if (diff==0)
			/* success: found it */
			return &dtd->tags[i];
	    }
	return NULL;
    }

/*________________________________________________________________________
**			Public Methods
*/


/*	Could check that we are back to bottom of stack! @@  */
PRIVATE int SGML_flush  (HTStream * context)
    {
	return (*context->actions->flush)(context->target);
    }

PRIVATE int SGML_free  (HTStream * context)
    {
	int status;

	if ((status = (*context->actions->_free)(context->target)) != HT_OK)
		return status;
	HTChunk_delete(context->string);
	HT_FREE(context);
	return HT_OK;
    }

PRIVATE int SGML_abort  (HTStream * context, HTList * e)
    {
	(*context->actions->abort)(context->target, e);
	HTChunk_delete(context->string);
	HT_FREE(context);
	return HT_ERROR;
    }

PRIVATE int SGML_write (HTStream * context, const char * b, int l)
    {
	const SGML_dtd	*dtd = context->dtd;
	HTChunk	*string = context->string;
	const char *text = b;
	int count = 0;
	
	while (l-- > 0)
	    {
		char c = *b++;
		switch(context->state)
		    {
		    got_element_open:
			/*
			** The label is jumped when the '>' of a the element
			** start tag has been detected. This DOES NOT FALL TO
			** THE CODE S_after_open, only processes the tag and
			** sets the state (c should still contain the
			** terminating character of the tag ('>'))
			*/
			if (context->current_tag && context->current_tag->name)
				start_element(context);
			context->state = S_after_open;
			break;

		    case S_after_open:
			/*
			** State S_after_open is entered only for single
			** character after the element opening tag to test
			** against newline. Strip one trainling newline only
			** after opening nonempty element.  - SGML: Ugh!
			*/
			text = b;
			count = 0;
			if (c == '\n' && (context->contents != SGML_EMPTY))
			    {
				context->state = S_text;
				break;
			    }
			--text;
			goto S_text;

		    S_text:
			context->state = S_text;
		    case S_text:
#ifdef ISO_2022_JP
			if (c == '\033')
			    {
				context->state = S_esc;
				++count;
				break;
			    }
#endif /* ISO_2022_JP */
			if (c == '&')
			    {
				if (count > 0)
					PUTB(text, count);
				count = 0;
				string->size = 0;
				context->state = S_ero;
			    }
			else if (c == '<')
			    {
				if (count > 0)
					PUTB(text, count);
				count = 0;
				string->size = 0;
				/* should scrap LITERAL, and use CDATA and
				   RCDATA -- msa */
				context->state =
					(context->contents == SGML_LITERAL) ?
						S_literal : S_tag;
			    }
			else if (c == '\n')
			    	/* Newline - ignore if before end tag! */
				context->state = S_nl;
			else
				++count;
			break;

		    case S_nl:
			if (c == '<')
			    {
				if (count > 0)
					PUTB(text, count);
				count = 0;
				string->size = 0;
				context->state =
					(context->contents == SGML_LITERAL) ?
						S_literal : S_nl_tago;
			    }
			else
			    {
				++count;
				goto S_text;
			    }
			break;

		    case S_nl_tago:	/* Had newline and tag opener */
			if (c != '/')
				PUTC('\n'); /* Only ignore newline before </ */
			context->state = S_tag;
			goto handle_S_tag;

#ifdef ISO_2022_JP
		    case S_esc:
			if (c=='$')
				context->state = S_dollar;
			else if (c=='(')
				context->state = S_paren;
			else
				context->state = S_text;
			++count;
			break;

		    case S_dollar:
			if (c=='@' || c=='B')
				context->state = S_nonascii_text;
			else
				context->state = S_text;
			++count;
			break;

		    case S_paren:
			if (c=='B' || c=='J')
				context->state = S_text;
			else
				context->state = S_text;
			++count;
			break;

		    case S_nonascii_text:
			if (c == '\033')
				context->state = S_esc;
			++count;
			break;
#endif /* ISO_2022_JP */

			/* In literal mode, waits only for specific end tag!
			** Only foir compatibility with old servers.
			*/
		    case S_literal:
			HTChunk_putc(string, c);
			if ( TOUPPER(c) !=
			    ((string->size == 1) ? '/'
			     : context->current_tag->name[string->size-2]))
			    {

				/* If complete match, end literal */
				if ((c == '>') &&
				    (!context->current_tag->name[string->size-2]))
				    {
					end_element
						(context,context->current_tag);
					/*
					  ...setting SGML_MIXED below is a
					  bit of kludge, but a good guess that
					  currently works, anything other than
					  SGML_LITERAL would work... -- msa */
					context->contents = SGML_MIXED;
				    }
				else
				    {
					/* If Mismatch: recover string. */
					PUTC( '<');
					PUTB(string->data, string->size);
				    }
				context->state = S_text;
				text = b;
				count = 0;
			    }
			break;

			/*
			** Character reference or Entity
			*/
		    case S_ero:
			if (c == '#')
			    {
				/*   &# is Char Ref Open */ 
				context->state = S_cro;
				break;
			    }
			context->state = S_entity;

			/** FALL THROUGH TO S_entity !! ***/

			/*
			** Handle Entities
			*/
		    case S_entity:
			if (isalnum((int) c))
				HTChunk_putc(string, c);
			else
			    {
				HTChunk_terminate(string);
				handle_entity(context);
				text = b;
				count = 0;
				if (c != ';')
				    {
					--text;
					goto S_text;
				    }
				context->state = S_text;
			    }
			break;

			/*	Character reference
			 */
		    case S_cro:
			if (isalnum((int)c))
				/* accumulate a character NUMBER */
				HTChunk_putc(string, c);
			else
			    {
				int value;
				HTChunk_terminate(string);
				if (sscanf(string->data, "%d", &value)==1)
					PUTC((char)value);
				else
				    {
					PUTB("&#", 2);
					PUTB(string->data, string->size-1);
				    }
				text = b;
				count = 0;
				if (c != ';')
				    {
					--text;
					goto S_text;
				    }
				context->state = S_text;
			    }
			break;

		    case S_tag:		/* new tag */
		    handle_S_tag:
			if (isalnum((int)c))
				HTChunk_putc(string, c);
			else
			    { /* End of tag name */
				int i;

				if (c == '/')
				    {
					if (string->size != 0)
						TRACE1("`<%s/' found!\n",
						       string->data);
					context->state = S_end;
					break;
				    }
				else if (c == '!')
				    {
					if (string->size != 0)
						TRACE1(" `<%s!' found!\n",
						       string->data);
					context->state = S_md;
					break;
				    }
				HTChunk_terminate(string);
				context->current_tag  = SGMLFindTag(dtd, string->data);
				if (context->current_tag == NULL)
					TRACE1("*** Unknown element %s\n",
					       string->data);
				else for (i=0;
					  i < context->current_tag->number_of_attributes; i++)
				    {
					context->present[i] = NO;
					context->value[i] = -1;
				    }
				context->token = string->size = 0;
				context->current_attribute_number = INVALID;
				goto S_tag_gap;
			    }
			break;

		    S_tag_gap:
			context->state = S_tag_gap;
		    case S_tag_gap:		/* Expecting attribute or > */
			if (isspace((int) c))
				break;	/* Gap between attributes */

			if (c == '>')
				goto got_element_open;
			else
				goto S_attr;

		    S_attr:
			/*
			** Start collecting the attribute name and collect
			** it in S_attr.
			*/
			context->state = S_attr;
			string->size = context->token;
		    case S_attr:
			if (isspace((int) c) || c == '>' || c == '=')
				goto got_attribute_name;
			else
				HTChunk_putc(string, c);
			break;

		    got_attribute_name:
			/*
			** This label is entered when attribute name has been
			** collected. Process it and enter S_attr_gap for
			** potential value or start of the next attribute.
			*/
			HTChunk_terminate(string) ;
			handle_attribute_name
				(context, string->data + context->token);
			string->size = context->token;
			context->state = S_attr_gap;
		    case S_attr_gap:	/* Expecting attribute or = or > */
			if (isspace((int) c))
				break;	/* Gap after attribute */

			if (c == '>')
				goto got_element_open;
			else if (c == '=')
				context->state = S_equals;
			else
				goto S_attr; /* Get next attribute */
			break;

		    case S_equals:	/* After attr = */ 
			if (isspace((int) c))
				break;	/* Before attribute value */

			if (c == '>')
			    {		/* End of tag */
				TRACE1("found = but no value\n", NULL);
				goto got_element_open;
			    }
			else if (c == '\'')
				context->state = S_squoted;
			else if (c == '"')
				context->state = S_dquoted;
			else
				goto S_value;
			break;

		    S_value:
			context->state = S_value;
			string->size = context->token;
		    case S_value:
			if (isspace((int) c) || c == '>')
			    {
				HTChunk_terminate(string);
				handle_attribute_value(context);
				context->token = string->size;
				goto S_tag_gap;
			    }
			else
				HTChunk_putc(string, c);
			break;
		
		    case S_squoted:	/* Quoted attribute value */
			if (c == '\'')
			    {
				HTChunk_terminate(string);
				handle_attribute_value(context);
				context->token = string->size;
				context->state = S_tag_gap;
			    }
			else if (c && c != '\n' && c != '\r')
				HTChunk_putc(string, c);
			break;
	
		    case S_dquoted:	/* Quoted attribute value */
			if (c == '"')
			    {
				HTChunk_terminate(string);
				handle_attribute_value(context);
				context->token = string->size;
				context->state = S_tag_gap;
			    }
			else if (c && c != '\n' && c != '\r')
				HTChunk_putc(string, c);
			break;

		    case S_end:	/* </ */
			if (isalnum((int) c))
				HTChunk_putc(string, c);
			else
			    {		/* End of end tag name */
				HTTag *t;

				HTChunk_terminate(string);
				if (*string->data)
					t = SGMLFindTag(dtd, string->data);
				else
				    	/* Empty end tag */
					/* Original code popped here one
					   from the stack. If this feature
					   is required, I have to put the
					   stack back... -- msa */
					t = NULL;
				if (!t)
					TRACE1("Unknown end tag </%s>\n",
					       string->data);
				else
				    {
					context->current_tag = NULL;
					end_element(context, t);
				    }
				string->size = 0;
				context->current_attribute_number = INVALID;
				if (c != '>')
				    {
					if (!isspace((int) c))
						TRACE2("`</%s%c' found!\n",
						       string->data, c);
					context->state = S_junk_tag;
				    }
				else
				    {
					text = b;
					count = 0;
					context->state = S_text;
				    }
			    }
			break;

		    S_junk_tag:
			context->state = S_junk_tag;
		    case S_junk_tag:
			if (c == '>')
			    {
				text = b;
				count = 0;
				context->state = S_text;
			    }
			break;

			/*
			** Scanning (actually skipping) declarations
			*/
		    case S_md:
			if (c == '-')
				context->state = S_com_1;
			else if (c == '"')
				context->state = S_md_dqs;
			else if (c == '\'')
				context->state = S_md_sqs;
			else if (c == '>')
			    {
				text = b;
				count = 0;
				context->state = S_text;
			    }
			break;

		    case S_md_dqs: /* Skip double quoted string */
			if (c == '"')
				context->state = S_md;
			break;

		    case S_md_sqs: /* Skip single quoted string */
			if (c == '\'')
				context->state = S_md;
			break;

		    case S_com_1: /* Starting a comment? */
			context->state = (c == '-') ? S_com : S_md;
			break;

		    case S_com: /* ..within comment */
			if (c == '-')
				context->state = S_com_2;
			break;

		    case S_com_2: /* Ending a comment ? */
			context->state = (c == '-') ? S_com_2a : S_com;
			break;
		    
		    case S_com_2a:
			if (c == '>') {
			    text = b;
			    count = 0;
			    context->state = S_text;
			} else
			    context->state = S_com;
			break;
		    }
	    }
	if (count > 0)
		PUTB(text, count);
	return HT_OK;
    }


PRIVATE int SGML_string (HTStream * context, const char* s)
    {
	return SGML_write(context, s, (int) strlen(s));
    }


PRIVATE int SGML_character (HTStream * context, char c)
    {
	return SGML_write(context, &c, 1);
    }

/*_______________________________________________________________________
*/

/*	Structured Object Class
**	-----------------------
*/
PRIVATE const HTStreamClass SGMLParser = 
    {		
	"SGMLParser",
	SGML_flush,
	SGML_free,
	SGML_abort,
	SGML_character, 
	SGML_string,
	SGML_write,
    }; 

/*	Create SGML Engine
**	------------------
**
** On entry,
**	dtd		represents the DTD, along with
**	actions		is the sink for the data as a set of routines.
**
*/
PUBLIC HTStream *SGML_new(const SGML_dtd * dtd, HTStructured * target)
    {
	int i;
	HTStream* context;
	if ((context = (HTStream  *) HT_CALLOC(1, sizeof(HTStream))) == NULL)
		HT_OUTOFMEM("SGML_begin");

	context->isa = &SGMLParser;
	context->string = HTChunk_new(128);	/* Grow by this much */
	context->dtd = dtd;
	context->target = target;
	context->actions = (HTStructuredClass*)(((HTStream*)target)->isa);
	                                    /* Ugh: no OO */
	context->state = S_text;
	for(i=0; i<MAX_ATTRIBUTES; i++)
		context->value[i] = 0;
	return context;
    }
