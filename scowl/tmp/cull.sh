#!/bin/bash

#How to generate cull file:

wl_exec=../../word_lists/word_list

cat words/* > a
$wl_exec sanitize a > b 2>/dev/null
sort -u b > a

cat not-words/* > x
$wl_exec sanitize x > y 2>/dev/null
sort -u y > x

$wl_exec cull x a > y 2>/dev/null
$wl_exec cull y cull_cull > cull 2>/dev/null
$wl_exec substring cull "'" > cull_contractions 2>/dev/null

rm -f a b x y

