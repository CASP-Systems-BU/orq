#!/bin/bash

cd $(dirname $0)/../

pip install -r requirements.txt

cd doc
doxygen
sphinx-build -b html -Dbreathe_projects.ORQ=xml . ./sphinx