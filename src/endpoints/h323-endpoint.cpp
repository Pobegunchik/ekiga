
/* Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2006 Damien Sandras
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
 *                         h323endpoint.cpp  -  description
 *                         --------------------------------
 *   begin                : Wed 24 Nov 2004
 *   copyright            : (C) 2000-2006 by Damien Sandras
 *   description          : This file contains the H.323 Endpoint class.
 *
 */


#include "config.h"

#include "h323-endpoint.h"

#include "opal-call.h"
#include "account-core.h"


namespace Opal {

  namespace H323 {

    class dialer : public PThread
    {
      PCLASSINFO(dialer, PThread);

  public:

      dialer (const std::string & uri, Opal::CallManager & _manager) 
        : PThread (1000, AutoDeleteThread), 
        dial_uri (uri),
        manager (_manager) 
      {
        this->Resume ();
      };

      void Main () 
        {
          PString token;
          manager.SetUpCall ("pc:*", dial_uri, token);
        };

  private:
      const std::string dial_uri;
      Opal::CallManager & manager;
    };

    class subscriber : public PThread
    {
      PCLASSINFO(subscriber, PThread);

  public:

      subscriber (const Opal::Account & _account,
                  Opal::H323::EndPoint::EndPoint & _manager) 
        : PThread (1000, AutoDeleteThread),
        account (_account),
        manager (_manager) 
      {
        this->Resume ();
      };

      void Main () 
        {
          manager.Register (account);
        };

  private:
      const Opal::Account & account;
      Opal::H323::EndPoint::EndPoint & manager;
    };
  };
};


/* The class */
Opal::H323::EndPoint::EndPoint (Opal::CallManager & _manager, Ekiga::ServiceCore & _core, unsigned _listen_port)
        : H323EndPoint (_manager), 
          manager (_manager),
          core (_core),
          runtime (*(dynamic_cast<Ekiga::Runtime *> (core.get ("runtime")))),
          account_core (*(dynamic_cast<Ekiga::AccountCore *> (core.get ("account-core"))))
{
  protocol_name = "h323";
  uri_prefix = "h323:";
  listen_port = _listen_port;

  /* Initial requested bandwidth */
  SetInitialBandwidth (40000);

  /* Start listener */
  set_listen_port (listen_port);

  /* Ready to take calls */
  manager.AddRouteEntry("h323:.* = pc:<db>");
  manager.AddRouteEntry("pc:.* = h323:<da>");
}


bool Opal::H323::EndPoint::populate_menu (Ekiga::Contact &contact,
                                          std::string uri,
                                          Ekiga::MenuBuilder &builder)
{
  return menu_builder_add_actions (contact.get_name (), uri, builder);
}


bool Opal::H323::EndPoint::populate_menu (Ekiga::Presentity& presentity,
                                          const std::string uri,
                                          Ekiga::MenuBuilder & builder)
{
  return menu_builder_add_actions (presentity.get_name (), uri, builder);
}


bool Opal::H323::EndPoint::menu_builder_add_actions (const std::string & /*fullname*/,
                                                     const std::string& uri,
                                                     Ekiga::MenuBuilder & builder)
{
  bool populated = false;
  std::string action = _("Call");

  builder.add_action ("call", action, sigc::bind (sigc::mem_fun (this, &EndPoint::on_dial), uri));

  populated = true;

  return populated;
}


bool Opal::H323::EndPoint::dial (const std::string & uri)
{
  if (uri.find ("h323:") == 0) {

    new dialer (uri, manager);

    return true;
  }

  return false;
}


const std::string & Opal::H323::EndPoint::get_protocol_name () const
{
  return protocol_name;
}


void Opal::H323::EndPoint::set_dtmf_mode (unsigned mode)
{
  switch (mode) 
    {
    case 0:
      SetSendUserInputMode (OpalConnection::SendUserInputAsString);
      break;
    case 1:
      SetSendUserInputMode (OpalConnection::SendUserInputAsTone);
      break;
    case 2:
      SetSendUserInputMode (OpalConnection::SendUserInputAsInlineRFC2833);
      break;
    case 3:
      SetSendUserInputMode (OpalConnection::SendUserInputAsQ931);
      break;
    default:
      break;
    }
}


unsigned Opal::H323::EndPoint::get_dtmf_mode () const
{
  if (GetSendUserInputMode () == OpalConnection::SendUserInputAsString)
    return 0;

  if (GetSendUserInputMode () == OpalConnection::SendUserInputAsTone)
    return 1;

  if (GetSendUserInputMode () == OpalConnection::SendUserInputAsInlineRFC2833)
    return 2;

  if (GetSendUserInputMode () == OpalConnection::SendUserInputAsQ931)
    return 2;

  return 1;
}


bool Opal::H323::EndPoint::set_listen_port (unsigned port)
{
  interface.protocol = "tcp";
  interface.interface = "*";

  if (port > 0) {

    std::stringstream str;
    RemoveListener (NULL);

    str << "tcp$*:" << port;
    if (StartListeners (PStringArray (str.str ()))) {

      interface.port = port;
      return true;
    }
  }

  return false;
}


const Ekiga::CallProtocolManager::Interface & Opal::H323::EndPoint::get_listen_interface () const
{
  return interface;
}


void Opal::H323::EndPoint::set_forward_uri (const std::string & uri)
{
  forward_uri = uri;
}


const std::string & Opal::H323::EndPoint::get_forward_uri () const
{
  return forward_uri;
}


bool Opal::H323::EndPoint::subscribe (const Opal::Account & account)
{
  if (account.get_protocol_name () != "H323")
    return false;

  new subscriber (account, *this);

  return true;
}


bool Opal::H323::EndPoint::unsubscribe (const Opal::Account & account)
{
  if (account.get_protocol_name () != "H323")
    return false;

  new subscriber (account, *this);

  return true;
}


void Opal::H323::EndPoint::Register (const Opal::Account & account)
{
  PString gatekeeperID;
  std::string info;
  std::string aor = account.get_aor ();

  bool unregister = !account.is_enabled ();

  if (!unregister && !IsRegisteredWithGatekeeper (account.get_host ())) {

    H323EndPoint::RemoveGatekeeper (0);

    if (!account.get_username ().empty ()) {
      SetLocalUserName (account.get_username ());
      AddAliasName (manager.GetDefaultDisplayName ());
    }

    SetGatekeeperPassword (account.get_password ());
    SetGatekeeperTimeToLive (account.get_timeout () * 1000);
    bool result = UseGatekeeper (account.get_host (), gatekeeperID);

    /* There was an error (missing parameter or registration failed)
       or the user chose to not register */
    if (!result) {

      /* Registering failed */
      if (gatekeeper) {

        switch (gatekeeper->GetRegistrationFailReason ()) {

        case H323Gatekeeper::DuplicateAlias :
          info = _("Duplicate alias");
          break;
        case H323Gatekeeper::SecurityDenied :
          info = _("Bad username/password");
          break;
        case H323Gatekeeper::TransportError :
          info = _("Transport error");
          break;
        case H323Gatekeeper::RegistrationSuccessful:
          break;
        case H323Gatekeeper::UnregisteredLocally:
        case H323Gatekeeper::UnregisteredByGatekeeper:
        case H323Gatekeeper::GatekeeperLostRegistration:
        case H323Gatekeeper::InvalidListener:
        case H323Gatekeeper::NumRegistrationFailReasons:
        case H323Gatekeeper::RegistrationRejectReasonMask:
        default :
          info = _("Failed");
          break;
        }
      }
      else
        info = _("Failed");

      /* Signal */
      runtime.run_in_main (sigc::bind (account.registration_event.make_slot (),
                                       Ekiga::AccountCore::RegistrationFailed,
                                       info));
    }
    else {

      runtime.run_in_main (sigc::bind (account.registration_event.make_slot (),
                                       Ekiga::AccountCore::Registered,
                                       std::string ()));
    }
  }
  else if (unregister && IsRegisteredWithGatekeeper (account.get_host ())) {

    H323EndPoint::RemoveGatekeeper (0);
    RemoveAliasName (account.get_username ());

    /* Signal */
    runtime.run_in_main (sigc::bind (account.registration_event.make_slot (),
                                     Ekiga::AccountCore::Unregistered,
                                     std::string ()));
  }
}


bool Opal::H323::EndPoint::UseGatekeeper (const PString & address,
                                          const PString & domain,
                                          const PString & iface)
{
  bool result = 
    H323EndPoint::UseGatekeeper (address, domain, iface);

  PWaitAndSignal m(gk_name_mutex);

  gk_name = address;

  return result;
}


bool Opal::H323::EndPoint::RemoveGatekeeper (const PString & address)
{
  if (IsRegisteredWithGatekeeper (address))
    return H323EndPoint::RemoveGatekeeper (0);

  return FALSE;
}


bool Opal::H323::EndPoint::IsRegisteredWithGatekeeper (const PString & address)
{
  PWaitAndSignal m(gk_name_mutex);

  return ((gk_name *= address) && H323EndPoint::IsRegisteredWithGatekeeper ());
}


bool Opal::H323::EndPoint::OnIncomingConnection (OpalConnection & connection,
                                                 G_GNUC_UNUSED unsigned options,
                                                 G_GNUC_UNUSED OpalConnection::StringOptions *stroptions)
{
  PTRACE (3, "EndPoint\tIncoming connection");

  if (!forward_uri.empty () && manager.get_unconditional_forward ())
    connection.ForwardCall (forward_uri);
  else if (manager.GetCallCount () > 1) { 

    if (!forward_uri.empty () && manager.get_forward_on_busy ())
      connection.ForwardCall (forward_uri);
    else {
      connection.ClearCall (OpalConnection::EndedByLocalBusy);
    }
  }
  else {

    Opal::Call *call = dynamic_cast<Opal::Call *> (&connection.GetCall ());
    if (call) {

      if (!forward_uri.empty () && manager.get_forward_on_no_answer ()) 
        call->set_no_answer_forward (manager.get_reject_delay (), forward_uri);
      else
        call->set_reject_delay (manager.get_reject_delay ());
    }

    return H323EndPoint::OnIncomingConnection (connection, options, stroptions);
  }

  return false;
}


void Opal::H323::EndPoint::on_dial (std::string uri)
{
  manager.dial (uri);
}