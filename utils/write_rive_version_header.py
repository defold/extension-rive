#!/usr/bin/env python

import os, sys, json, pprint
import requests

# from urllib.error import HTTPError, URLError
# from urllib.request import urlopen, Request

URL="https://api.github.com/repos/rive-app/rive-runtime/git/commits/%s"

def get(url, headers = None):
    try:
        response = requests.get(url, headers=headers or {})
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

# gitpython: https://stackoverflow.com/questions/65076306/how-to-get-git-commit-hash-of-python-project-using-python-code
def GetCommitDetails(sha1):
    url = URL % sha1
    print("URL", url)

    headers = {}
    headers['Accept'] = 'application/vnd.github+json'
    headers['X-GitHub-Api-Version'] = '2022-11-28'

    return get(url, headers)

def WriteHeader(path, info):
    with open(path, 'w') as f:
        f.write("// Generated. Do not edit! See ./utils/write_rive_version_header.py\n")
        f.write("\n")

        committer = info['committer']
        message = info['message']
        lines = ['//' + line for line in message.split('\n')]
        f.write('\n'.join(lines))
        f.write("\n")

        f.write('const char* RIVE_RUNTIME_AUTHOR=\"%s\";\n' % committer['name'])
        f.write('const char* RIVE_RUNTIME_DATE=\"%s\";\n' % committer['date'])
        f.write('const char* RIVE_RUNTIME_SHA1=\"%s\";\n' % info['sha'])

    print(f"Wrote {path}")


def Usage():
    sys.exit(1)

if __name__ == "__main__":
    print("ARGS", sys.argv)
    if len(sys.argv) < 2:
        Usage()

    path = sys.argv[1]
    sha1 = sys.argv[2]
    info = GetCommitDetails(sha1)
    WriteHeader(path, info)
