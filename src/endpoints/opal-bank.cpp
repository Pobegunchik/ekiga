
/* Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2007 Damien Sandras
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * Ekiga is licensed under the GPL license and as a special exception,
 * you have permission to link or otherwise combine this program with the
 * programs OPAL, OpenH323 and PWLIB, and distribute the combination,
 * without applying the requirements of the GNU GPL to the OPAL, OpenH323
 * and PWLIB programs, as long as you do follow the requirements of the
 * GNU GPL for all the rest of the software thus combined.
 */


/*
 *                         opal-account.cpp  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Damien Sandras
 *   copyright            : (c) 2008 by Damien Sandras
 *   description          : implementation of an opal account
 *
 */

#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>

#include "config.h"

#include "common.h"

#include "menu-builder.h"

#include "opal-bank.h"
#include "form-request-simple.h"


bool Opal::Bank::populate_menu (Ekiga::MenuBuilder & builder)
{
  builder.add_action ("new", _("_New"),
		      sigc::mem_fun (this, &Opal::Bank::new_account));

  return true;
}


void Opal::Bank::new_account ()
{
  Ekiga::FormRequestSimple request;

  request.title (_("Edit account"));

  request.instructions (_("Please update the following fields:"));

  request.text ("name", _("Name:"), std::string ());
  request.text ("host", _("Host"), std::string ());
  request.text ("user", _("User:"), std::string ());
  request.text ("authentication_user", _("Authentication User:"), std::string ());
  request.private_text ("password", _("Password:"), std::string ());
  request.text ("timeout", _("Timeout:"), std::string ());
  request.boolean ("enabled", _("Enable Account"), true);

  request.submitted.connect (sigc::mem_fun (this, &Opal::Bank::on_new_account_form_submitted));

  if (!questions.handle_request (&request)) {

    // FIXME: better error reporting
#ifdef __GNUC__
    std::cout << "Unhandled form request in "
	      << __PRETTY_FUNCTION__ << std::endl;
#endif
  }
}


void Opal::Bank::on_new_account_form_submitted (Ekiga::Form &result)
{
  try {

    Ekiga::FormRequestSimple request;

    std::string error;
    std::string new_name = result.text ("name");
    std::string new_host = result.text ("host");
    std::string new_user = result.text ("user");
    std::string new_authentication_user = result.text ("authentication_user");
    std::string new_password = result.private_text ("password");
    bool new_enabled = result.boolean ("enabled");
    unsigned new_timeout = atoi (result.text ("timeout").c_str ());

    result.visit (request);

    if (new_name.empty ()) 
      error = _("You did not supply a name for that account.");
    else if (new_host.empty ()) 
      error = _("You did not supply a host to register to.");
    else if (new_user.empty ())
      error = _("You did not supply a user name for that account.");

    if (!error.empty ()) {
      request.error (error);
      request.submitted.connect (sigc::mem_fun (this, &Opal::Bank::on_new_account_form_submitted));

      if (!questions.handle_request (&request)) {
#ifdef __GNUC__
        std::cout << "Unhandled form request in "
          << __PRETTY_FUNCTION__ << std::endl;
#endif
      }
    }
    else {

      add (new_name, new_host, new_user, new_authentication_user, new_password, new_enabled, new_timeout);
      save ();
    }

  } catch (Ekiga::Form::not_found) {

    std::cerr << "Invalid result form" << std::endl; // FIXME: do better
  }
}


void Opal::Bank::add (std::string name, 
                      std::string host,
                      std::string user,
                      std::string auth_user,
                      std::string password,
                      bool enabled,
                      unsigned timeout)
{
  Opal::Account *account = new Opal::Account (core, name, host, user, auth_user, password, enabled, timeout);

  add_account (*account);
}