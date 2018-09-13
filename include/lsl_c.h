#ifndef LSL_C_H
#define LSL_C_H

/**
* C API for the lab streaming layer.
* 
* The lab streaming layer provides a set of functions to make instrument data accessible 
* in real time within a lab network. From there, streams can be picked up by recording programs, 
* viewing programs or custom experiment applications that access data streams in real time.
*
* The API covers two areas:
* - The "push API" allows to create stream outlets and to push data (regular or irregular measurement 
*   time series, event data, coded audio/video frames, etc.) into them.
* - The "pull API" allows to create stream inlets and read time-synched experiment data from them 
*   (for recording, viewing or experiment control).
*
* To use this library you need to link to either the liblsl32 or liblsl64 shared library that comes with
* this header. Under Visual Studio the library is linked in automatically.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

/* ===================================================== */
/* ==== Objects provided by the lab streaming layer ==== */
/* ===================================================== */

/**
* Handle to a stream info object. Stores the declaration of a data stream.
* Represents the following information:
*  a) stream data format (#channels, channel format)
*  b) core information (stream name, content type, sampling rate)
*  c) optional meta-data about the stream content (channel labels, measurement units, etc.)
*
* Whenever a program wants to provide a new stream on the lab network it will typically first 
* create an lsl_streaminfo to describe its properties and then construct an lsl_outlet with it to create
* the stream on the network. Other parties who discover/resolve the outlet on the network can query the 
* stream info; it is also written to disk when recording the stream (playing a similar role as a file header).
*/
typedef struct lsl_streaminfo_struct_* lsl_streaminfo;

/**
* A stream outlet handle.
* Outlets are used to make streaming data (and the meta-data) available on the lab network.
*/
typedef struct lsl_outlet_struct_* lsl_outlet;

/**
* A stream inlet handle.
* Inlets are used to receive streaming data (and meta-data) from the lab network.
*/
typedef struct lsl_inlet_struct_* lsl_inlet;

/**
* A lightweight XML element tree handle; models the description of a streaminfo object.
* XML elements behave like advanced pointers into memory that is owned by some respective streaminfo.
* Has a name and can have multiple named children or have text content as value; attributes are omitted.
* Insider note: The interface is modeled after a subset of pugixml's node type and is compatible with it.
* Type-casts between pugi::xml_node_struct* and lsl_xml_ptr are permitted (in both directions) since the types 
* are binary compatible.
* See also pugixml.googlecode.com/svn/tags/latest/docs/manual/access.html for additional documentation.
*/
typedef struct lsl_xml_ptr_struct_* lsl_xml_ptr;

/** 
* Handle to a convenience object that resolves streams continuously in the background throughout 
* its lifetime and which can be queried at any time for the set of streams that are currently 
* visible on the network.
*/
typedef struct lsl_continuous_resolver_* lsl_continuous_resolver;



/* ============================================================ */
/* ==== Free functions provided by the lab streaming layer ==== */
/* ============================================================ */

#include "freefuncs.h"


/* =============================== */
/* ==== lsl_streaminfo object ==== */
/* =============================== */

#include "streaminfo.h"

/* =========================== */
/* ==== lsl_outlet object ==== */
/* =========================== */

#include "outlet.h"

/* ========================== */
/* ==== lsl_inlet object ==== */
/* ========================== */

#include "inlet.h"


/* ============================ */
/* ==== lsl_xml_ptr object ==== */
/* ============================ */

#include "xml_element.h"

/* ======================================== */
/* ==== lsl_continuous_resolver object ==== */
/* ======================================== */

#include "continuous_resolver.h"

#ifdef __cplusplus
} /* end extern "C" */
#endif


#endif

