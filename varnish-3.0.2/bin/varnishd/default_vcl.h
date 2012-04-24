/*
 * NB:  This file is machine generated, DO NOT EDIT!
 *
 * Edit default.vcl instead and run make
 *
 */

 "/*-\n"
 " * Copyright (c) 2006 Verdens Gang AS\n"
 " * Copyright (c) 2006-2011 Varnish Software AS\n"
 " * All rights reserved.\n"
 " *\n"
 " * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>\n"
 " *\n"
 " * Redistribution and use in source and binary forms, with or without\n"
 " * modification, are permitted provided that the following conditions\n"
 " * are met:\n"
 " * 1. Redistributions of source code must retain the above copyright\n"
 " *    notice, this list of conditions and the following disclaimer.\n"
 " * 2. Redistributions in binary form must reproduce the above copyright\n"
 " *    notice, this list of conditions and the following disclaimer in the\n"
 " *    documentation and/or other materials provided with the distribution.\n"
 " *\n"
 " * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND\n"
 " * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
 " * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n"
 " * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE\n"
 " * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR\n"
 " * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF\n"
 " * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR \n"
 " * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,\n"
 " * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE\n"
 " * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,\n"
 " * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
 " *\n"
 " * The default VCL code.\n"
 " *\n"
 " * NB! You do NOT need to copy & paste all of these functions into your\n"
 " * own vcl code, if you do not provide a definition of one of these\n"
 " * functions, the compiler will automatically fall back to the default\n"
 " * code from this file.\n"
 " *\n"
 " * This code will be prefixed with a backend declaration built from the\n"
 " * -b argument.\n"
 " */\n"
 "\n"
 "sub vcl_recv {\n"
 "    if (req.restarts == 0) {\n"
 "	if (req.http.x-forwarded-for) {\n"
 "	    set req.http.X-Forwarded-For =\n"
 "		req.http.X-Forwarded-For + \", \" + client.ip;\n"
 "	} else {\n"
 "	    set req.http.X-Forwarded-For = client.ip;\n"
 "	}\n"
 "    }\n"
 "    if (req.request != \"GET\" &&\n"
 "      req.request != \"HEAD\" &&\n"
 "      req.request != \"PUT\" &&\n"
 "      req.request != \"POST\" &&\n"
 "      req.request != \"TRACE\" &&\n"
 "      req.request != \"OPTIONS\" &&\n"
 "      req.request != \"DELETE\") {\n"
 "        /* Non-RFC2616 or CONNECT which is weird. */\n"
 "        return (pipe);\n"
 "    }\n"
 "    if (req.request != \"GET\" && req.request != \"HEAD\") {\n"
 "        /* We only deal with GET and HEAD by default */\n"
 "        return (pass);\n"
 "    }\n"
 "    if (req.http.Authorization || req.http.Cookie) {\n"
 "        /* Not cacheable by default */\n"
 "        return (pass);\n"
 "    }\n"
 "    return (lookup);\n"
 "}\n"
 "\n"
 "sub vcl_pipe {\n"
 "    # Note that only the first request to the backend will have\n"
 "    # X-Forwarded-For set.  If you use X-Forwarded-For and want to\n"
 "    # have it set for all requests, make sure to have:\n"
 "    # set bereq.http.connection = \"close\";\n"
 "    # here.  It is not set by default as it might break some broken web\n"
 "    # applications, like IIS with NTLM authentication.\n"
 "    return (pipe);\n"
 "}\n"
 "\n"
 "sub vcl_pass {\n"
 "    return (pass);\n"
 "}\n"
 "\n"
 "sub vcl_hash {\n"
 "    hash_data(req.url);\n"
 "    if (req.http.host) {\n"
 "        hash_data(req.http.host);\n"
 "    } else {\n"
 "        hash_data(server.ip);\n"
 "    }\n"
 "    return (hash);\n"
 "}\n"
 "\n"
 "sub vcl_hit {\n"
 "    return (deliver);\n"
 "}\n"
 "\n"
 "sub vcl_miss {\n"
 "    return (fetch);\n"
 "}\n"
 "\n"
 "sub vcl_fetch {\n"
 "    if (beresp.ttl <= 0s ||\n"
 "        beresp.http.Set-Cookie ||\n"
 "        beresp.http.Vary == \"*\") {\n"
 "		/*\n"
 "		 * Mark as \"Hit-For-Pass\" for the next 2 minutes\n"
 "		 */\n"
 "		set beresp.ttl = 120 s;\n"
 "		return (hit_for_pass);\n"
 "    }\n"
 "    return (deliver);\n"
 "}\n"
 "\n"
 "sub vcl_deliver {\n"
 "    return (deliver);\n"
 "}\n"
 "\n"
 "sub vcl_error {\n"
 "    set obj.http.Content-Type = \"text/html; charset=utf-8\";\n"
 "    set obj.http.Retry-After = \"5\";\n"
 "    synthetic {\"\n"
 "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
 "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n"
 " \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
 "<html>\n"
 "  <head>\n"
 "    <title>\"} + obj.status + \" \" + obj.response + {\"</title>\n"
 "  </head>\n"
 "  <body>\n"
 "    <h1>Error \"} + obj.status + \" \" + obj.response + {\"</h1>\n"
 "    <p>\"} + obj.response + {\"</p>\n"
 "    <h3>Guru Meditation:</h3>\n"
 "    <p>XID: \"} + req.xid + {\"</p>\n"
 "    <hr>\n"
 "    <p>Varnish cache server</p>\n"
 "  </body>\n"
 "</html>\n"
 "\"};\n"
 "    return (deliver);\n"
 "}\n"
 "\n"
 "sub vcl_init {\n"
 "	return (ok);\n"
 "}\n"
 "\n"
 "sub vcl_fini {\n"
 "	return (ok);\n"
 "}\n"