#!/bin/sh
BCM_PTT_PIN=17
BCM_SQL_PIN=27

gpio -g mode $BCM_PTT_PIN up
gpio -g mode $BCM_SQL_PIN out

gpio export $BCM_PTT_PIN in
gpio export $BCM_SQL_PIN out
