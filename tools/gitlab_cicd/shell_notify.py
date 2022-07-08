# -*- coding:utf-8 -*-

import requests
import json
import argparse
import importlib
import sys
import io
import gitlab
import json
import urllib
import time
import hashlib
import http.client
import logging

logging.captureWarnings(True)

importlib.reload(sys)
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')


class PushDataObj:
    def __init__(self):
        self.push2UserListUrl = ""
        self.push2GroupUrl = ""
        self.token = ""
        self.appId = ""
        self.appkey = ""
        self.title = ""
        self.pushType = "RTX"
        self.userList = ""
        self.ipList = ""
        self.ccList = ""
        self.merger = "0"
        self.content = ""
        self.noteType = "1"
        self.t = str(time.time())
        self.extra = {}
        self.sign = ""

    def setPush2UserListUrl(self, push2UserListUrl):
        self.push2UserListUrl = push2UserListUrl

    def setPush2GroupUrl(self, push2GroupUrl):
        self.push2GroupUrl = push2GroupUrl

    def setToken(self, token):
        self.token = token

    def setAppId(self, appId):
        self.appId = appId

    def setAppkey(self, appkey):
        self.appkey = appkey

    def setTitle(self, title):
        self.title = title

    def setPushType(self, pushType):
        self.pushType = pushType

    def setUserList(self, userList):
        self.userList = userList

    def setIpList(self, ipList):
        self.ipList = ipList

    def setCcList(self, ccList):
        self.ccList = ccList

    def setMerger(self, merger):
        self.merger = merger

    def setContent(self, content):
        self.content = content

    def setNoteType(self, noteType):
        self.noteType = noteType

    def setExtra(self, extra):
        self.extra = extra

    def calc_sign(self):
        md5_str = self.appId + self.title + self.pushType + self.userList + self.ipList + \
            self.ccList + self.merger + self.content + self.noteType + self.t + self.appkey

        self.sign = hashlib.md5(md5_str.encode('utf-8')).hexdigest()

    @staticmethod
    def postData(apiUrl, data):
        headers = {"Content-Type": "text/plain"}
        params = json.dumps(data)
        conn = http.client.HTTPSConnection('push.sec.wanmei.net:20176')
        conn.request('POST', apiUrl, params, headers)
        res = conn.getresponse()
        content = res.read()
        resjson = json.loads(content)
        return dict(resjson)

    def sendData2UserList(self):
        jsonstr = {
            "appId": self.appId,
            "title": self.title,
            "pushType": self.pushType,
            "userList": self.userList,
            "ipList": self.ipList,
            "ccList": self.ccList,
            "merge": self.merger,
            "content": self.content,
            "noteType": self.noteType,
            "t": self.t,
            "extra": self.extra,
            "sign": self.sign
        }

        print(jsonstr)
        res = PushDataObj.postData(self.push2UserListUrl, jsonstr)
        return res

    def sendData2Group(self):
        jsonstr = {
            "sender": "gitlab_blueprint_server_cicd",
            "content": self.title + '\n' + self.content,
            "userId": "gitlab_blueprint_server_cicd",
            "groupName": "蓝图服务器构建通知"
        }

        response = requests.post(self.push2GroupUrl, headers={
            'Content-Type': 'application/json'}, data=json.dumps(jsonstr), verify=False)

        return response.text

    # [项目组名、项目名、提交号]
    def get_gitlab_info(self, project_url, project_name, commit_sha):
        url = urllib.parse.urlparse(project_url)
        git_url = url.scheme + "://" + url.netloc + "/"
        # print(git_url)
        gl = gitlab.Gitlab(git_url,
                           private_token=self.token)
        projects = gl.projects.list(search=project_name)
        for project in projects:
            if project.path_with_namespace == project_url:
                break

        commit = project.commits.get(commit_sha)
        return commit

    def parse_args(self):
        parser = argparse.ArgumentParser()
        gitlab_setting = parser.add_argument_group('gitlab setting')

        gitlab_setting.add_argument('-os', '--gitlab_os', dest='gitlab_os', type=str,
                                    help='系统', default='')

        gitlab_setting.add_argument('-pgu', '--push2group_url', dest='push2group_url', type=str,
                                    help='post 有度群地址', default='')

        gitlab_setting.add_argument('-puu', '--push2userlist_url', dest='push2userlist_url', type=str,
                                    help='post 有度用户地址', default='')

        gitlab_setting.add_argument('-tok', '--token', dest='token', type=str,
                                    help='私有token', default='')

        gitlab_setting.add_argument('-aid', '--appid', dest='appid', type=str,
                                    help='推送ID', default='')

        gitlab_setting.add_argument('-apk', '--appkey', dest='appkey', type=str,
                                    help='推送key', default='')

        gitlab_setting.add_argument('-apt', '--push_type', dest='push_type', type=str,
                                    help='推送类型', default='')

        gitlab_setting.add_argument('-rt', '--gitlab_result', dest='gitlab_result', type=str,
                                    help='构建结果', default='')

        gitlab_setting.add_argument('-us', '--user_name', dest='user_name', type=str,
                                    help='构建者', default='')

        gitlab_setting.add_argument('-pr', '--project_name', dest='project_name', type=str,
                                    help='项目名称', default='')

        gitlab_setting.add_argument('-cs', '--commit_sha', dest='commit_sha', type=str,
                                    help='提交号', default='')

        gitlab_setting.add_argument('-rn', '--commit_ref_name', dest='commit_ref_name', type=str,
                                    help='项目的分支名或tag名', default='')

        gitlab_setting.add_argument('-bj', '--gitlab_jobid', dest='gitlab_jobid', type=str,
                                    help='当前作业的ID', default='')

        gitlab_setting.add_argument('-pu', '--project_url', dest='project_url', type=str,
                                    help='项目名称http地址', default='')

        gitlab_setting.add_argument('-pi', '--pipeline_id', dest='pipeline_id', type=str,
                                    help='当前流水线ID', default='')

        gitlab_setting.add_argument('-pwj', '--win_publish_jobid', dest='win_publish_jobid', type=int,
                                    help='publish win作业ID', default=0)

        gitlab_setting.add_argument('-pe', '--publish_expire', dest='publish_expire', type=str,
                                    help='publish 有效期', default='')

        return parser

    def command_line_args(self, args):
        parser = self.parse_args()
        args = parser.parse_args(args)

        if not args:
            parser.print_help()
            sys.exit(1)

        return args


if __name__ == '__main__':

    pdObjs = PushDataObj()

    args = pdObjs.command_line_args(sys.argv[1:])
    print(args)

    pdObjs.setPush2GroupUrl(args.push2group_url)
    pdObjs.setPush2UserListUrl(args.push2userlist_url)
    pdObjs.setToken(args.token)
    pdObjs.setAppId(args.appid)
    pdObjs.setAppkey(args.appkey)
    pdObjs.setPushType(args.push_type)

    commit = pdObjs.get_gitlab_info(
        args.project_url, args.project_name, args.commit_sha)

    pdObjs.setUserList(commit.author_email.strip("@pwrd.com"))

    job_url = args.project_url + "/-/jobs/" + args.gitlab_jobid
    failed_job_url = args.project_url + "/pipelines/" + args.pipeline_id + "/failures"
    win_artifacts_url = args.project_url + "/-/jobs/" \
        + str(args.win_publish_jobid) + "/artifacts/download"

    linux_artifacts_url = args.project_url + "/-/jobs/" \
        + str(args.win_publish_jobid - 1) + "/artifacts/download"

    gitlab_info = {"GITLAB_OS": args.gitlab_os, "GITLAB_RESULT": args.gitlab_result, "GITLAB_USER_NAME": args.user_name, "CI_PROJECT_NAME": args.project_name,
                   "CI_COMMIT_SHA": args.commit_sha, "CI_COMMIT_REF_NAME": args.commit_ref_name,  "GITLAB_CI_POST_EXPIRE": args.publish_expire, "CI_COMMIT_MESSAGE": commit.message.rstrip('\n'), "CI_AUTHOR_EMAIL": commit.author_email, "CI_COMMITTED_DATE": commit.committed_date,
                   "CI_PIPELINE_URL": job_url, "CI_PIPELINE_FAILED_JOBS": failed_job_url, "CI_WIN_ARTIFACTS_URL": win_artifacts_url, "CI_LINUX_ARTIFACTS_URL": linux_artifacts_url}

    print(gitlab_info)

    if args.gitlab_result == "失败":
        title = "你的git提交违法规定,快去修复问题!!!"
        content = "蓝图服务器构建结果:\n \
warning:{GITLAB_RESULT}\n \
本次构建由:[{GITLAB_USER_NAME}]触发\n \
项目名称:{CI_PROJECT_NAME}\n \
提交号:{CI_COMMIT_SHA}\n \
提交日志:{CI_COMMIT_MESSAGE}\n \
构建分支:{CI_COMMIT_REF_NAME}\n \
构建者Email:{CI_AUTHOR_EMAIL}\n \
构建者commit时间:{CI_COMMITTED_DATE}\n \
检查失败日志地址:{CI_PIPELINE_FAILED_JOBS}\n \
你上传的c,c++,lua代码语法,格式化有问题,请去[检查失败日志地址]查看原因,并及时修正上传".format(**gitlab_info)

    elif args.gitlab_result == "成功":
        title = "你的git提交成功 ^.^"
        content = "蓝图服务器构建结果:\n \
warning:{GITLAB_RESULT}\n \
本次构建由:[{GITLAB_USER_NAME}]触发\n \
项目名称:{CI_PROJECT_NAME}\n \
提交号:{CI_COMMIT_SHA}\n \
提交日志:{CI_COMMIT_MESSAGE}\n \
构建分支:{CI_COMMIT_REF_NAME}\n \
构建者Email:{CI_AUTHOR_EMAIL}\n \
构建者commit时间:{CI_COMMITTED_DATE}\n \
win服务器下载地址:{CI_WIN_ARTIFACTS_URL}\n \
linux服务器下载地址:{CI_LINUX_ARTIFACTS_URL}\n \
太棒了!你的代码publish成功啦 ^.^ 请及时去[服务器下载地址]下载最新编译程序\n \
有效期{GITLAB_CI_POST_EXPIRE}".format(**gitlab_info)
    else:
        title = "你的git提交失败,系统异常,请联系运维或者程序进行系统修复"
        content = "蓝图服务器构建结果:\n \
warning:{GITLAB_RESULT}\n \
本次构建由:[{GITLAB_USER_NAME}]触发\n \
项目名称:{CI_PROJECT_NAME}\n \
提交号:{CI_COMMIT_SHA}\n \
提交日志:{CI_COMMIT_MESSAGE}\n \
构建分支:{CI_COMMIT_REF_NAME}\n \
构建者Email:{CI_AUTHOR_EMAIL}\n \
构建者commit时间:{CI_COMMITTED_DATE}\n \
编译后的工程publish失败,请联系运维或程序管理员进行检查修正".format(**gitlab_info)

    pdObjs.setTitle(title)
    pdObjs.setContent(content)
    pdObjs.calc_sign()
    print(pdObjs.sendData2UserList())
    print(pdObjs.sendData2Group())
