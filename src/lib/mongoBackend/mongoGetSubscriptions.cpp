/*
*
* Copyright 2015 Telefonica Investigacion y Desarrollo, S.A.U
*
* This file is part of Orion Context Broker.
*
* Orion Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* iot_support at tid dot es
*
* Author: Orion dev team
*/

#include "common/sem.h"

#include "logMsg/logMsg.h"
#include "logMsg/traceLevels.h"

#include "mongoBackend/mongoGetSubscriptions.h"
#include "mongoBackend/MongoGlobal.h"
#include "mongoBackend/connectionOperations.h"

#include "mongo/client/dbclient.h"

using namespace ngsiv2;

/* ****************************************************************************
*
* setSubscriptionId -
*/
static void setSubscriptionId(Subscription* s, const BSONObj& r)
{
  s->id = r.getField("_id").OID().toString();
}


/* ****************************************************************************
*
* setSubject -
*/
static void setSubject(Subscription* s, const BSONObj& r)
{
  // Entities
  std::vector<BSONElement> ents = r.getField(CSUB_ENTITIES).Array();
  for (unsigned int ix = 0; ix < ents.size(); ++ix)
  {
    BSONObj ent           = ents[ix].embeddedObject();
    std::string id        = ent.getStringField(CSUB_ENTITY_ID);
    std::string type      = ent.getStringField(CSUB_ENTITY_TYPE);
    std::string isPattern = ent.getStringField(CSUB_ENTITY_ISPATTERN);

    EntID en;
    if (isFalse(isPattern))
    {
      en.id = id;
    }
    else
    {
      en.idPattern = id;
    }
    en.type = type;

    s->subject.entities.push_back(en);
  }

  // Condition
  std::vector<BSONElement> conds = r.getField(CSUB_CONDITIONS).Array();
  for (unsigned int ix = 0; ix < conds.size(); ++ix)
  {
    BSONObj cond = conds[ix].embeddedObject();
    // The ONCHANGE check is needed, as a subscription could mix different conditions types in DB
    if (std::string(cond.getStringField(CSUB_CONDITIONS_TYPE)) == "ONCHANGE")
    {
      std::vector<BSONElement> condValues = cond.getField(CSUB_CONDITIONS_VALUE).Array();
      for (unsigned int jx = 0; jx < condValues.size(); ++jx)
      {
        std::string attr = condValues[jx].String();
        s->subject.condition.attributes.push_back(attr);
      }
    }
  }

  // Note that current DB model is based on NGSIv1 and doesn't consider expressions. Thus
  // subject.condition.expression cannot be filled. The implemetion will be enhanced once
  // the DB model gets defined
  // TBD

}


/* ****************************************************************************
*
* setNotification -
*/
static void setNotification(Subscription* s, const BSONObj& r)
{
  // Attributes
  std::vector<BSONElement> attrs = r.getField(CSUB_ATTRS).Array();
  for (unsigned int ix = 0; ix < attrs.size(); ++ix)
  {
    std::string attr = attrs[ix].String();
    s->notification.attributes.push_back(attr);
  }

  // Callback
  s->notification.callback = r.getStringField(CSUB_REFERENCE);

  // Throttling
  s->notification.throttling = r.hasField(CSUB_THROTTLING)? r.getField(CSUB_THROTTLING).numberLong() : -1;

  // Last Notification
  s->notification.lastNotification = r.hasField(CSUB_LASTNOTIFICATION) ? r.getField(CSUB_LASTNOTIFICATION).numberLong() : -1;

  // Count
  s->notification.timesSent = r.hasField(CSUB_COUNT) ? r.getField(CSUB_COUNT).numberLong() : -1;

}


/* ****************************************************************************
*
* setExpires -
*/
static void setExpires(Subscription* s, const BSONObj& r)
{
  // Last Notification
  s->expires = r.hasField(CSUB_EXPIRATION) ? r.getField(CSUB_EXPIRATION).numberLong() : -1;

  // Status
  // FIXME P10: use a enum for this
  s->status = s->expires > getCurrentTime() ? "active" : "expired";

}

/* ****************************************************************************
*
* mongoListSubscriptions -
*/
void mongoListSubscriptions
(
  std::vector<Subscription>            *subs,
  OrionError                           *oe,
  std::map<std::string, std::string>&  uriParam,
  const std::string&                   tenant,
  int                                  limit,
  int                                  offset
)
{

  bool           reqSemTaken = false;
  long long      count       = 0LL;

  reqSemTake(__FUNCTION__, "Mongo List Subscriptions", SemReadOp, &reqSemTaken);

  LM_T(LmtMongo, ("Mongo List Subscriptions"));

  /* ONTIMEINTERVAL subscription are not part of NGSIv2, so they are excluded.
   * Note that expiration is not taken into account (in the future, a q= query
   * could be added to the operation in order to filter results) */
  std::auto_ptr<DBClientCursor>  cursor;
  std::string                    err;
  std::string                    conds = std::string(CSUB_CONDITIONS) + "." + CSUB_CONDITIONS_TYPE;
  Query                          q     = Query(BSON(conds << "ONCHANGE"));

  q.sort(BSON("_id" << 1));

  if (!collectionRangedQuery(getSubscribeContextCollectionName(tenant), q, limit, offset, &cursor, &count, &err))
  {
    reqSemGive(__FUNCTION__, "Mongo List Subscriptions", reqSemTaken);
    *oe = OrionError(SccReceiverInternalError, err);
    return;
  }

  /* Process query result */
  while (cursor->more())
  {
    BSONObj r = cursor->next();
    LM_T(LmtMongo, ("retrieved document: '%s'", r.toString().c_str()));

    Subscription s;
    setSubscriptionId(&s, r);
    setSubject(&s, r);
    setNotification(&s, r);
    setExpires(&s, r);
    subs->push_back(s);
  }

  reqSemGive(__FUNCTION__, "Mongo List Subscriptions", reqSemTaken);
  *oe = OrionError(SccOk);
  return;
}


/* ****************************************************************************
*
* mongoGetSubscription -
*/
void mongoGetSubscription
(
  ngsiv2::Subscription*               sub,
  OrionError*                         oe,
  const std::string&                  idSub,
  std::map<std::string, std::string>& uriParam,
  const std::string&                  tenant
)
{
  bool  reqSemTaken = false;

  reqSemTake(__FUNCTION__, "Mongo Get Subscription", SemReadOp, &reqSemTaken);

  LM_T(LmtMongo, ("Mongo Get Subscription"));

  std::auto_ptr<DBClientCursor>  cursor;
  std::string                    err;
  BSONObj                        q     = BSON("_id" << OID(idSub));

  if (!collectionQuery(getSubscribeContextCollectionName(tenant), q, &cursor, &err))
  {
    reqSemGive(__FUNCTION__, "Mongo Get Subscription", reqSemTaken);
    *oe = OrionError(SccReceiverInternalError, err);
    return;
  }

  /* Process query result */
  if (cursor->more())
  {
    BSONObj r = cursor->next();
    LM_T(LmtMongo, ("retrieved document: '%s'", r.toString().c_str()));

    setSubscriptionId(sub, r);
    setSubject(sub, r);
    setNotification(sub, r);
    setExpires(sub, r);

    if (cursor->more())
    {
      // Ooops, we expect only one
      LM_T(LmtMongo, ("more than one subscription: '%s'", idSub.c_str()));
      reqSemGive(__FUNCTION__, "Mongo Get Subscription", reqSemTaken);
      *oe = OrionError(SccConflict);
      return;
    }
  }
  else
  {
    LM_T(LmtMongo, ("subscription not found: '%s'", idSub.c_str()));
    reqSemGive(__FUNCTION__, "Mongo Get Subscription", reqSemTaken);
    *oe = OrionError(SccSubscriptionIdNotFound);
    return;
  }

  reqSemGive(__FUNCTION__, "Mongo Get Subscription", reqSemTaken);
  *oe = OrionError(SccOk);
  return;
}