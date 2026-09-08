import tempfile
import unittest
from pathlib import Path

from scripts.validate_repo import (
    discover_agents,
    find_forbidden_references,
    is_kebab_case,
    validate_agent_metadata,
    validate_repository,
)


VALID_METADATA = """\
schema_version: 1
name: sample-agent
version: 0.1.0
execution_mode: local
runtime: custom
entrypoint: ./run.sh
contract_version: 1
model:
  provider: example
  name: model
environment: []
tags: []
"""


class ValidateRepoTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.agents = self.root / "agents"
        self.agents.mkdir()

    def tearDown(self):
        self.temp.cleanup()

    def make_agent(self, name="sample-agent", metadata=VALID_METADATA):
        agent = self.agents / name
        agent.mkdir()
        (agent / "agent.yaml").write_text(metadata, encoding="utf-8")
        run = agent / "run.sh"
        run.write_text("#!/usr/bin/env sh\nexit 0\n", encoding="utf-8")
        return agent

    def test_discovers_non_private_agent_directories(self):
        agent = self.make_agent()
        self.assertEqual(discover_agents(self.agents), [agent])

    def test_ignores_template_directory(self):
        (self.agents / "_template").mkdir()
        self.assertEqual(discover_agents(self.agents), [])

    def test_rejects_non_kebab_case_agent_name(self):
        self.assertFalse(is_kebab_case("Bad_Agent"))
        self.assertTrue(is_kebab_case("qwen3-local-agent"))

    def test_requires_agent_yaml(self):
        agent = self.agents / "missing-metadata"
        agent.mkdir()
        self.assertIn("missing agent.yaml", validate_agent_metadata(agent)[0])

    def test_requires_declared_entrypoint_to_exist(self):
        metadata = VALID_METADATA.replace("./run.sh", "./missing.sh")
        agent = self.make_agent(metadata=metadata)
        errors = validate_agent_metadata(agent)
        self.assertTrue(any("does not exist" in error for error in errors))

    def test_rejects_entrypoint_outside_agent_directory(self):
        outside = self.root / "outside.sh"
        outside.write_text("exit 0\n", encoding="utf-8")
        metadata = VALID_METADATA.replace("./run.sh", "../outside.sh")
        agent = self.make_agent(metadata=metadata)
        errors = validate_agent_metadata(agent)
        self.assertTrue(any("escapes agent directory" in error for error in errors))

    def test_rejects_relative_reference_to_evaluation(self):
        agent = self.make_agent()
        (agent / "main.py").write_text("path = '../evaluation/data.json'\n", encoding="utf-8")
        errors = find_forbidden_references(agent)
        self.assertTrue(any("../evaluation/data.json" in error for error in errors))

    def test_rejects_relative_reference_to_scripts(self):
        agent = self.make_agent()
        (agent / "main.py").write_text("path = '../scripts/helper.py'\n", encoding="utf-8")
        errors = find_forbidden_references(agent)
        self.assertTrue(any("../scripts/helper.py" in error for error in errors))

    def test_rejects_relative_reference_to_other_agent(self):
        agent = self.make_agent()
        (agent / "main.py").write_text("path = '../other-agent/config.json'\n", encoding="utf-8")
        errors = find_forbidden_references(agent)
        self.assertTrue(any("../other-agent/config.json" in error for error in errors))

    def test_rejects_multi_level_escape(self):
        agent = self.make_agent()
        src = agent / "src"
        src.mkdir()
        (src / "main.py").write_text("path = '../../evaluation/data.json'\n", encoding="utf-8")
        errors = find_forbidden_references(agent)
        self.assertTrue(any("../../evaluation/data.json" in error for error in errors))

    def test_allows_relative_reference_that_stays_inside_agent(self):
        agent = self.make_agent()
        src = agent / "src"
        config = agent / "config"
        src.mkdir()
        config.mkdir()
        (src / "main.py").write_text("path = '../config/settings.json'\n", encoding="utf-8")
        self.assertEqual(find_forbidden_references(agent), [])

    def test_valid_repository_has_no_errors(self):
        self.make_agent()
        self.assertEqual(validate_repository(self.root), [])

    def test_broken_agent_makes_repository_invalid(self):
        self.make_agent("Bad_Agent")
        errors = validate_repository(self.root)
        self.assertTrue(any("lowercase kebab-case" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
