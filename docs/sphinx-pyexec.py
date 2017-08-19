try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO

import sys
from docutils.parsers.rst import Directive
from docutils import nodes
from sphinx.util import nested_parse_with_titles
from docutils.statemachine import StringList


class ExecDirective(Directive):
    has_content = True
    required_arguments = 0

    def execute_code(cls, code):
        codeOut = StringIO()
        codeErr = StringIO()

        sys.stdout = codeOut
        sys.stderr = codeErr

        exec(code)

        sys.stdout = sys.__stdout__
        sys.stderr = sys.__stderr__

        results = list()
        results.append(codeOut.getvalue())
        results.append(codeErr.getvalue())
        results = ''.join(results)

        return results

    def run(self):
        self.assert_has_content()

        code = '\n'.join(self.content)
        code_results = self.execute_code(code)

        sl = StringList(code_results.replace("\r", "").split("\n"))

        node = nodes.paragraph()
        nested_parse_with_titles(self.state, sl, node)

        output = []
        output.append(node)
        return output


def setup(app):
    app.add_directive('exec', ExecDirective)
