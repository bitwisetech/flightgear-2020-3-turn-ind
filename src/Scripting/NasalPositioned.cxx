// NasalPositioned.cxx -- expose FGPositioned classes to Nasal
//
// Written by James Turner, started 2012.
//
// Copyright (C) 2012 James Turner
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>

#include "NasalPositioned.hxx"

#include <boost/foreach.hpp>

#include <simgear/scene/material/mat.hxx>
#include <simgear/magvar/magvar.hxx>
#include <simgear/timing/sg_time.hxx>
#include <simgear/bucket/newbucket.hxx>

#include <Airports/runways.hxx>
#include <Airports/simple.hxx>
#include <Navaids/navlist.hxx>
#include <Navaids/procedure.hxx>
#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Scenery/scenery.hxx>
#include <ATC/CommStation.hxx>

static void ghostDestroy(void* g);
naGhostType PositionedGhostType = { ghostDestroy, "positioned" };

static naRef stringToNasal(naContext c, const std::string& s)
{
    return naStr_fromdata(naNewString(c),
                   const_cast<char *>(s.c_str()), 
                   s.length());
}

static FGPositioned* positionedGhost(naRef r)
{
    if (naGhost_type(r) == &PositionedGhostType)
        return (FGPositioned*) naGhost_ptr(r);
    return 0;
}


static void ghostDestroy(void* g)
{
    FGPositioned* pos = (FGPositioned*)g;
    SGReferenced::put(pos); // unref
}

static naRef airportPrototype;

naRef ghostForPositioned(naContext c, const FGPositioned* pos)
{
    if (!pos) {
        return naNil();
    }
    
    SGReferenced::get(pos); // take a ref
    return naNewGhost(c, &PositionedGhostType, (void*) pos);
}

naRef hashForAirport(naContext c, const FGAirport* apt)
{
    std::string id = apt->ident();
    std::string name = apt->name();
    
  // build runways hash
    naRef rwys = naNewHash(c);
    for(unsigned int r=0; r<apt->numRunways(); ++r) {
      FGRunway* rwy(apt->getRunwayByIndex(r));
      naRef rwyid = stringToNasal(c, rwy->ident());
      naRef rwydata = hashForRunway(c, rwy);
      naHash_set(rwys, rwyid, rwydata);
    }
  
    naRef aptdata = naNewHash(c);
#define HASHSET(s,l,n) naHash_set(aptdata, naStr_fromdata(naNewString(c),s,l),n)
    HASHSET("id", 2, stringToNasal(c, id));
    HASHSET("name", 4, stringToNasal(c, name));
    HASHSET("lat", 3, naNum(apt->getLatitude()));
    HASHSET("lon", 3, naNum(apt->getLongitude()));
    HASHSET("elevation", 9, naNum(apt->getElevation() * SG_FEET_TO_METER));
    HASHSET("has_metar", 9, naNum(apt->getMetar()));
    HASHSET("runways", 7, rwys);
    
    HASHSET("_positioned", 11, ghostForPositioned(c, apt));
    
    naRef parents = naNewVector(c);
    naVec_append(parents, airportPrototype);
    HASHSET("parents", 7, parents);
#undef HASHSET
    
    return aptdata;
}

naRef hashForRunway(naContext c, FGRunway* rwy)
{
    naRef rwyid = stringToNasal(c, rwy->ident());
    naRef rwydata = naNewHash(c);
#define HASHSET(s,l,n) naHash_set(rwydata, naStr_fromdata(naNewString(c),s,l),n)
    HASHSET("id", 2, rwyid);
    HASHSET("lat", 3, naNum(rwy->latitude()));
    HASHSET("lon", 3, naNum(rwy->longitude()));
    HASHSET("heading", 7, naNum(rwy->headingDeg()));
    HASHSET("length", 6, naNum(rwy->lengthM()));
    HASHSET("width", 5, naNum(rwy->widthM()));
    HASHSET("threshold", 9, naNum(rwy->displacedThresholdM()));
    HASHSET("stopway", 7, naNum(rwy->stopwayM()));
        
    if (rwy->ILS()) {
      HASHSET("ils_frequency_mhz", 17, naNum(rwy->ILS()->get_freq() / 100.0));
      HASHSET("ils", 3, hashForNavRecord(c, rwy->ILS(), SGGeod()));
    }
        
    naRef sidVec = naNewVector(c);
        
    BOOST_FOREACH(flightgear::SID* sid, rwy->getSIDs()) {
      naRef procId = stringToNasal(c, sid->ident());
      naVec_append(sidVec, procId);
    }
    HASHSET("sids", 4, sidVec); 
        
    naRef starVec = naNewVector(c);      
    BOOST_FOREACH(flightgear::STAR* star, rwy->getSTARs()) {
      naRef procId = stringToNasal(c, star->ident());
      naVec_append(starVec, procId);
    }
    HASHSET("stars", 5, starVec); 
    
    HASHSET("_positioned", 11, ghostForPositioned(c, rwy));
#undef HASHSET
    return rwydata;
}

naRef hashForNavRecord(naContext c, const FGNavRecord* nav, const SGGeod& rel)
{
    naRef navdata = naNewHash(c);
#define HASHSET(s,l,n) naHash_set(navdata, naStr_fromdata(naNewString(c),s,l),n)
    HASHSET("id", 2, stringToNasal(c, nav->ident()));
    HASHSET("name", 4, stringToNasal(c, nav->name()));
    HASHSET("frequency", 9, naNum(nav->get_freq()));
    HASHSET("lat", 3, naNum(nav->get_lat()));
    HASHSET("lon", 3, naNum(nav->get_lon()));
    HASHSET("elevation", 9, naNum(nav->get_elev_ft() * SG_FEET_TO_METER));
    HASHSET("type", 4, stringToNasal(c, nav->nameForType(nav->type())));
    
// FIXME - get rid of these, people should use courseAndDistance instead
    HASHSET("distance", 8, naNum(SGGeodesy::distanceNm( rel, nav->geod() ) * SG_NM_TO_METER ) );
    HASHSET("bearing", 7, naNum(SGGeodesy::courseDeg( rel, nav->geod() ) ) );
    
    // record the real object as a ghost for further operations
    HASHSET("_positioned",11, ghostForPositioned(c, nav));
#undef HASHSET
    
    return navdata;
}

bool geodFromHash(naRef ref, SGGeod& result)
{
  if (!naIsHash(ref)) {
    return false;
  }
  
// first, see if the hash contains a FGPositioned ghost - in which case
// we can read off its position directly
  naRef posGhost = naHash_cget(ref, (char*) "_positioned");
  if (!naIsNil(posGhost)) {
    FGPositioned* pos = positionedGhost(posGhost);
    result = pos->geod();
    return true;
  }
  
// then check for manual latitude / longitude names
  naRef lat = naHash_cget(ref, (char*) "lat");
  naRef lon = naHash_cget(ref, (char*) "lon");
  if (naIsNum(lat) && naIsNum(lon)) {
    result = SGGeod::fromDeg(naNumValue(lat).num, naNumValue(lon).num);
    return true;
  }
  
// check for geo.Coord type
    
// check for any synonyms?
    // latitude + longitude?
  
  return false;
}

// Convert a cartesian point to a geodetic lat/lon/altitude.
static naRef f_carttogeod(naContext c, naRef me, int argc, naRef* args)
{
  double lat, lon, alt, xyz[3];
  if(argc != 3) naRuntimeError(c, "carttogeod() expects 3 arguments");
  for(int i=0; i<3; i++)
    xyz[i] = naNumValue(args[i]).num;
  sgCartToGeod(xyz, &lat, &lon, &alt);
  lat *= SG_RADIANS_TO_DEGREES;
  lon *= SG_RADIANS_TO_DEGREES;
  naRef vec = naNewVector(c);
  naVec_append(vec, naNum(lat));
  naVec_append(vec, naNum(lon));
  naVec_append(vec, naNum(alt));
  return vec;
}

// Convert a geodetic lat/lon/altitude to a cartesian point.
static naRef f_geodtocart(naContext c, naRef me, int argc, naRef* args)
{
  if(argc != 3) naRuntimeError(c, "geodtocart() expects 3 arguments");
  double lat = naNumValue(args[0]).num * SG_DEGREES_TO_RADIANS;
  double lon = naNumValue(args[1]).num * SG_DEGREES_TO_RADIANS;
  double alt = naNumValue(args[2]).num;
  double xyz[3];
  sgGeodToCart(lat, lon, alt, xyz);
  naRef vec = naNewVector(c);
  naVec_append(vec, naNum(xyz[0]));
  naVec_append(vec, naNum(xyz[1]));
  naVec_append(vec, naNum(xyz[2]));
  return vec;
}

// For given geodetic point return array with elevation, and a material data
// hash, or nil if there's no information available (tile not loaded). If
// information about the material isn't available, then nil is returned instead
// of the hash.
static naRef f_geodinfo(naContext c, naRef me, int argc, naRef* args)
{
#define HASHSET(s,l,n) naHash_set(matdata, naStr_fromdata(naNewString(c),s,l),n)
  if(argc < 2 || argc > 3)
    naRuntimeError(c, "geodinfo() expects 2 or 3 arguments: lat, lon [, maxalt]");
  double lat = naNumValue(args[0]).num;
  double lon = naNumValue(args[1]).num;
  double elev = argc == 3 ? naNumValue(args[2]).num : 10000;
  const SGMaterial *mat;
  SGGeod geod = SGGeod::fromDegM(lon, lat, elev);
  if(!globals->get_scenery()->get_elevation_m(geod, elev, &mat))
    return naNil();
  naRef vec = naNewVector(c);
  naVec_append(vec, naNum(elev));
  naRef matdata = naNil();
  if(mat) {
    matdata = naNewHash(c);
    naRef names = naNewVector(c);
    BOOST_FOREACH(const std::string& n, mat->get_names())
      naVec_append(names, stringToNasal(c, n));
      
    HASHSET("names", 5, names);
    HASHSET("solid", 5, naNum(mat->get_solid()));
    HASHSET("friction_factor", 15, naNum(mat->get_friction_factor()));
    HASHSET("rolling_friction", 16, naNum(mat->get_rolling_friction()));
    HASHSET("load_resistance", 15, naNum(mat->get_load_resistance()));
    HASHSET("bumpiness", 9, naNum(mat->get_bumpiness()));
    HASHSET("light_coverage", 14, naNum(mat->get_light_coverage()));
  }
  naVec_append(vec, matdata);
  return vec;
#undef HASHSET
}


class AirportInfoFilter : public FGAirport::AirportFilter
{
public:
  AirportInfoFilter() : type(FGPositioned::AIRPORT) {
  }
  
  virtual FGPositioned::Type minType() const {
    return type;
  }
  
  virtual FGPositioned::Type maxType() const {
    return type;
  }
  
  FGPositioned::Type type;
};

// Returns data hash for particular or nearest airport of a <type>, or nil
// on error.
//
// airportinfo(<id>);                   e.g. "KSFO"
// airportinfo(<type>);                 type := ("airport"|"seaport"|"heliport")
// airportinfo()                        same as  airportinfo("airport")
// airportinfo(<lat>, <lon> [, <type>]);
static naRef f_airportinfo(naContext c, naRef me, int argc, naRef* args)
{
  SGGeod pos;
  FGAirport* apt = NULL;
  
  if(argc >= 2 && naIsNum(args[0]) && naIsNum(args[1])) {
    pos = SGGeod::fromDeg(args[1].num, args[0].num);
    args += 2;
    argc -= 2;
  } else {
    pos = globals->get_aircraft_position();
  }
  
  double maxRange = 10000.0; // expose this? or pick a smaller value?
  
  AirportInfoFilter filter; // defaults to airports only
  
  if(argc == 0) {
    // fall through and use AIRPORT
  } else if(argc == 1 && naIsString(args[0])) {
    const char *s = naStr_data(args[0]);
    if(!strcmp(s, "airport")) filter.type = FGPositioned::AIRPORT;
    else if(!strcmp(s, "seaport")) filter.type = FGPositioned::SEAPORT;
    else if(!strcmp(s, "heliport")) filter.type = FGPositioned::HELIPORT;
    else {
      // user provided an <id>, hopefully
      apt = FGAirport::findByIdent(s);
      if (!apt) {
        // return nil here, but don't raise a runtime error; this is a
        // legitamate way to validate an ICAO code, for example in a
        // dialog box or similar.
        return naNil();
      }
    }
  } else {
    naRuntimeError(c, "airportinfo() with invalid function arguments");
    return naNil();
  }
  
  if(!apt) {
    apt = FGAirport::findClosest(pos, maxRange, &filter);
    if(!apt) return naNil();
  }
  
  return hashForAirport(c, apt);
}

static FGAirport* airportFromRef(naRef ref)
{
    if (naIsString(ref)) {
        return FGAirport::findByIdent(naStr_data(ref));
    }
 
    FGPositioned* pos = positionedGhost(ref);
    if (pos && FGAirport::isAirportType(pos)) {
        return (FGAirport*) pos;
    }

    return NULL;
}

static naRef f_airport_tower(naContext c, naRef me, int argc, naRef* args)
{
    FGPositioned* pos = positionedGhost(me);
    if (!pos ||! FGAirport::isAirportType(pos)) {
        naRuntimeError(c, "airport_tower called on non-airport object");
    }
    
    FGAirport* apt = (FGAirport*) pos;
    
    // build a hash for the tower position    
    SGGeod towerLoc = apt->getTowerLocation();
    naRef tower = naNewHash(c);
#define HASHSET(s,l,n) naHash_set(tower, naStr_fromdata(naNewString(c),s,l),n)
    HASHSET("lat", 3, naNum(towerLoc.getLatitudeDeg()));
    HASHSET("lon", 3, naNum(towerLoc.getLongitudeDeg()));
    HASHSET("elevation", 9, naNum(towerLoc.getElevationM()));
#undef HASHSET
    return tower;
}

static naRef f_airport_comms(naContext c, naRef me, int argc, naRef* args)
{
    FGPositioned* pos = positionedGhost(me);
    if (!pos ||! FGAirport::isAirportType(pos)) {
        naRuntimeError(c, "airport_comms called on non-airport object");
    }
    
    FGAirport* apt = (FGAirport*) pos;
    naRef comms;
    
// if we have an explicit type, return a simple vector of frequencies
    if (argc > 0 && naIsScalar(args[0])) {
        std::string commName = naStr_data(args[0]);
        FGPositioned::Type commType = FGPositioned::typeFromName(commName);
        
        naRef comms = naNewVector(c);
        BOOST_FOREACH(flightgear::CommStation* comm, apt->commStationsOfType(commType)) {
            naVec_append(comms, naNum(comm->freqMHz()));
        }
    } else {
// otherwise return a vector of hashes, one for each comm station.
        BOOST_FOREACH(flightgear::CommStation* comm, apt->commStations()) {
            naRef commHash = naNewHash(c);
#define HASHSET(s,n) naHash_set(commHash, naStr_fromdata(naNewString(c),s,strlen(s)),n)
            HASHSET("frequency", naNum(comm->freqMHz()));
            HASHSET("ident", stringToNasal(c, comm->ident()));
            
#undef HASHSET
            naVec_append(comms, commHash);
        }
    }
    
    return comms;

}

// Returns vector of data hash for navaid of a <type>, nil on error
// navaids sorted by ascending distance 
// navinfo([<lat>,<lon>],[<type>],[<id>])
// lat/lon (numeric): use latitude/longitude instead of ac position
// type:              ("fix"|"vor"|"ndb"|"ils"|"dme"|"tacan"|"any")
// id:                (partial) id of the fix
// examples:
// navinfo("vor")     returns all vors
// navinfo("HAM")     return all navaids who's name start with "HAM"
// navinfo("vor", "HAM") return all vor who's name start with "HAM"
//navinfo(34,48,"vor","HAM") return all vor who's name start with "HAM" 
//                           sorted by distance relative to lat=34, lon=48
static naRef f_navinfo(naContext c, naRef me, int argc, naRef* args)
{
  SGGeod pos;
  
  if(argc >= 2 && naIsNum(args[0]) && naIsNum(args[1])) {
    pos = SGGeod::fromDeg(args[1].num, args[0].num);
    args += 2;
    argc -= 2;
  } else {
    pos = globals->get_aircraft_position();
  }
  
  FGPositioned::Type type = FGPositioned::INVALID;
  nav_list_type navlist;
  const char * id = "";
  
  if(argc > 0 && naIsString(args[0])) {
    const char *s = naStr_data(args[0]);
    if(!strcmp(s, "any")) type = FGPositioned::INVALID;
    else if(!strcmp(s, "fix")) type = FGPositioned::FIX;
    else if(!strcmp(s, "vor")) type = FGPositioned::VOR;
    else if(!strcmp(s, "ndb")) type = FGPositioned::NDB;
    else if(!strcmp(s, "ils")) type = FGPositioned::ILS;
    else if(!strcmp(s, "dme")) type = FGPositioned::DME;
    else if(!strcmp(s, "tacan")) type = FGPositioned::TACAN;
    else id = s; // this is an id
    ++args;
    --argc;
  } 
  
  if(argc > 0 && naIsString(args[0])) {
    if( *id != 0 ) {
      naRuntimeError(c, "navinfo() called with navaid id");
      return naNil();
    }
    id = naStr_data(args[0]);
    ++args;
    --argc;
  }
  
  if( argc > 0 ) {
    naRuntimeError(c, "navinfo() called with too many arguments");
    return naNil();
  }
  
  navlist = globals->get_navlist()->findByIdentAndFreq( pos, id, 0.0, type );
  
  naRef reply = naNewVector(c);
  for( nav_list_type::const_iterator it = navlist.begin(); it != navlist.end(); ++it ) {
    naVec_append( reply, hashForNavRecord(c, *it, pos) );
  }
  return reply;
}

// Convert a cartesian point to a geodetic lat/lon/altitude.
static naRef f_magvar(naContext c, naRef me, int argc, naRef* args)
{
  SGGeod pos = globals->get_aircraft_position();
  if (argc == 0) {
    // fine, use aircraft position
  } else if ((argc == 1) && geodFromHash(args[0], pos)) {
    // okay
  } else if ((argc == 2) && naIsNum(args[0]) && naIsNum(args[1])) {
    double lat = naNumValue(args[0]).num,
      lon = naNumValue(args[1]).num;
    pos = SGGeod::fromDeg(lon, lat);
  } else {
    naRuntimeError(c, "magvar() expects no arguments, a positioned hash or lat,lon pair");
  }
  
  double jd = globals->get_time_params()->getJD();
  double magvarDeg = sgGetMagVar(pos, jd);
  return naNum(magvarDeg);
}

static naRef f_courseAndDistance(naContext c, naRef me, int argc, naRef* args)
{
    SGGeod from = globals->get_aircraft_position(), to;
    if ((argc == 1) && geodFromHash(args[0], to)) {
        // done
    } else if ((argc == 2) && naIsNum(args[0]) && naIsNum(args[1])) {
        // two number arguments, from = current pos, to = lat+lon
        double lat = naNumValue(args[0]).num,
            lon = naNumValue(args[1]).num;
        to = SGGeod::fromDeg(lon, lat);
    } else if ((argc == 2) && geodFromHash(args[0], from) && geodFromHash(args[1], to)) {
        // done
    } else if ((argc == 3) && geodFromHash(args[0], from) && naIsNum(args[1]) && naIsNum(args[2])) {
        double lat = naNumValue(args[1]).num,
            lon = naNumValue(args[2]).num;
        to = SGGeod::fromDeg(lon, lat);
    } else if ((argc == 3) && naIsNum(args[0]) && naIsNum(args[1]) && geodFromHash(args[2], to)) {
        double lat = naNumValue(args[0]).num,
            lon = naNumValue(args[1]).num;
        from = SGGeod::fromDeg(lon, lat);
    } else if (argc == 4) {
        if (!naIsNum(args[0]) || !naIsNum(args[1]) || !naIsNum(args[2]) || !naIsNum(args[3])) {
            naRuntimeError(c, "invalid arguments to courseAndDistance - expected four numbers");
        }
        
        from = SGGeod::fromDeg(naNumValue(args[1]).num, naNumValue(args[0]).num);
        to = SGGeod::fromDeg(naNumValue(args[3]).num, naNumValue(args[2]).num);
    } else {
        naRuntimeError(c, "invalid arguments to courseAndDistance");
    }
    
    double course, course2, d;
    SGGeodesy::inverse(from, to, course, course2, d);
    
    naRef result = naNewVector(c);
    naVec_append(result, naNum(course));
    naVec_append(result, naNum(d));
    return result;
}

static naRef f_tilePath(naContext c, naRef me, int argc, naRef* args)
{
    SGGeod pos = globals->get_aircraft_position();
    if (argc == 0) {
        // fine, use aircraft position
    } else if ((argc == 1) && geodFromHash(args[0], pos)) {
        // okay
    } else if ((argc == 2) && naIsNum(args[0]) && naIsNum(args[1])) {
        double lat = naNumValue(args[0]).num,
        lon = naNumValue(args[1]).num;
        pos = SGGeod::fromDeg(lon, lat);
    } else {
        naRuntimeError(c, "bucketPath() expects no arguments, a positioned hash or lat,lon pair");
    }
    
    SGBucket b(pos);
    return stringToNasal(c, b.gen_base_path());
}

// Table of extension functions.  Terminate with zeros.
static struct { const char* name; naCFunction func; } funcs[] = {
  { "carttogeod", f_carttogeod },
  { "geodtocart", f_geodtocart },
  { "geodinfo", f_geodinfo },
  { "airportinfo", f_airportinfo },
  { "navinfo", f_navinfo },
  { "magvar", f_magvar },
  { "courseAndDistance", f_courseAndDistance },
  { "bucketPath", f_tilePath },
  { 0, 0 }
};

static void hashset(naContext c, naRef hash, const char* key, naRef val)
{
  naRef s = naNewString(c);
  naStr_fromdata(s, (char*)key, strlen(key));
  naHash_set(hash, s, val);
}

naRef initNasalPositioned(naRef globals, naContext c)
{
    airportPrototype = naNewHash(c);
    hashset(c, airportPrototype, "tower", naNewFunc(c, naNewCCode(c, f_airport_tower)));
    hashset(c, airportPrototype, "comms", naNewFunc(c, naNewCCode(c, f_airport_comms)));
    
  for(int i=0; funcs[i].name; i++) {
    hashset(c, globals, funcs[i].name,
            naNewFunc(c, naNewCCode(c, funcs[i].func)));
  }
  
  return naNil();
}
