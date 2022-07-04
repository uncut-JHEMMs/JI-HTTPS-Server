#!/usr/bin/env bash

extract()
{
  _path=$1
  _fields=$2

  cat $_path | cut -d',' -f$_fields | tail -n+2 | sort | uniq
}

extract $1 1 | tr '\n' ' ' | sed 's/ *$//' > data/users.ssv
extract $1 1,2 | tr '\n' ' ' | sed 's/ *$//' > data/cards.ssv
extract $1 9,13 | tr '\n' ' ' | sed 's/ *$//' > data/merchants.ssv
extract $1 9-12 > data/merchant_locations.ssv