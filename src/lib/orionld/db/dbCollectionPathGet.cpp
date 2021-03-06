/*
*
* Copyright 2019 FIWARE Foundation e.V.
*
* This file is part of Orion-LD Context Broker.
*
* Orion-LD Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion-LD Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion-LD Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* orionld at fiware dot org
*
* Author: Ken Zangelin
*/
#include "logMsg/logMsg.h"                                       // LM_*
#include "logMsg/traceLevels.h"                                  // Lmt*

#include "orionld/common/orionldState.h"                         // orionldState, dbName
#include "orionld/common/orionldErrorResponse.h"                 // orionldErrorResponseCreate
#include "orionld/db/dbCollectionPathGet.h"                      // Own interface



// ----------------------------------------------------------------------------
//
// dbCollectionPathGet -
//
int dbCollectionPathGet(char* path, int pathLen, const char* collection)
{
  int tenantLen     = ((multitenancy == true) && (orionldState.tenant != NULL) && (orionldState.tenant[0] != 0))? strlen(orionldState.tenant) + 1 : 0;
  int collectionLen = strlen(collection) + 1;  // +1: '.'

  if (dbNameLen + tenantLen + collectionLen >= pathLen)
  {
    LM_E(("Internal Error (database name is too long)"));
    orionldErrorResponseCreate(OrionldBadRequestData, "Database Error", "Unable to compose collection name - name too long");
    return -1;
  }

  strcpy(path, dbName);

  if (tenantLen != 0)
  {
    path[dbNameLen] = '-';
    strcpy(&path[dbNameLen + 1], orionldState.tenant);
  }

  path[dbNameLen + tenantLen] = '.';
  strcpy(&path[dbNameLen + tenantLen + 1], collection);

  return 0;
}



// ----------------------------------------------------------------------------
//
// dbCollectionPathGetWithTenant -
//
int dbCollectionPathGetWithTenant(char* path, int pathLen, const char* tenant, const char* collection)
{
  int tenantLen     = strlen(tenant);
  int collectionLen = strlen(collection) + 1;  // +1: '.'

  if (tenantLen + collectionLen >= pathLen)
  {
    LM_E(("Internal Error (database name is too long)"));
    orionldErrorResponseCreate(OrionldBadRequestData, "Database Error", "Unable to compose collection name - name too long");
    return -1;
  }

  strcpy(path, dbName);

  snprintf(path, pathLen, "%s.%s", tenant, collection);

  return 0;
}
