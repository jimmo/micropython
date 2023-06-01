#!/usr/bin/env python3
#
# This file is part of the MicroPython project, http://micropython.org/
#
# The MIT License (MIT)
#
# Copyright (c) 2023 Jim Mussared
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import re
from typing import List, Optional
from gitlint.git import GitCommit
from gitlint.rules import (
    LineRule,
    CommitRule,
    RuleViolation,
    CommitMessageBody,
    CommitMessageTitle,
)


class MicroPythonTitleLineRule(LineRule):
    name = "micropython-title-line"
    id = "UC1"
    target = CommitMessageTitle

    def validate(self, line: str, _commit: GitCommit) -> Optional[List[RuleViolation]]:
        title = line.split(":", 1)

        if len(title) == 1 or not title[1].startswith(" "):
            return [RuleViolation(self.id, 'Title does not start with "path: "', line_nr=1)]

        subject = title[1][1:]

        if not subject.endswith("."):
            return [RuleViolation(self.id, 'Subject does not end with a "."', line_nr=1)]

        if not subject[0].isupper():
            return [
                RuleViolation(self.id, "Subject does not start with a capital letter.", line_nr=1)
            ]

        if len(subject.split(" ")) < 2:
            return [RuleViolation(self.id, "Subject must contain more than one word.", line_nr=1)]


class MicroPythonAuthorRule(CommitRule):
    name = "micropython-author"
    id = "UC2"

    def validate(self, commit: GitCommit) -> Optional[List[RuleViolation]]:
        if "noreply" in commit.author_name or "noreply" in commit.author_email:
            return [
                RuleViolation(
                    self.id,
                    'Invalid email address "{} <{}>".'.format(
                        commit.author_name, commit.author_email
                    ),
                    line_nr=1,
                )
            ]


class MicroPythonBodyMaxLineLength(LineRule):
    name = "micropython-body-max-line-length"
    id = "UC3"
    max_length = 75
    target = CommitMessageBody

    def validate(self, line: str, _commit: GitCommit) -> Optional[List[RuleViolation]]:
        # The email address might be long.
        if line.startswith("Signed-off-by: "):
            return

        # Allow long lines containing URLs.
        if "://" in line:
            return

        if len(line) > self.max_length:
            return [
                RuleViolation(
                    self.id,
                    "Line exceeds max length ({0}>{1})".format(len(line), self.max_length),
                    line,
                )
            ]
