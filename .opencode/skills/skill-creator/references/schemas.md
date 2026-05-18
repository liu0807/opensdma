# JSON Schema 参考

定义 skill-creator 使用的 JSON 结构。

---

## evals.json

位于 skill 目录下的 `evals/evals.json`。

```json
{
  "skill_name": "example-skill",
  "evals": [
    {
      "id": 1,
      "prompt": "用户的示例 prompt",
      "name": "e2e-basic-form-fill",
      "expected_output": "对预期结果的描述",
      "files": ["evals/files/sample1.pdf"],
      "assertions": [
        "输出包含 X",
        "技能使用了脚本 Y"
      ]
    }
  ]
}
```

**字段：**
- `skill_name`: 匹配 skill frontmatter 中的 name
- `evals[].id`: 唯一整数标识符
- `evals[].name`: 描述性名称（用作目录名，如 "e2e-basic-form-fill"）
- `evals[].prompt`: 要执行的任务
- `evals[].expected_output`: 可读的成功描述
- `evals[].files`: 可选的输入文件路径列表（相对于 skill 根目录）
- `evals[].assertions`: 可验证的声明列表

---

## eval_metadata.json

每次 eval 运行时，在运行目录中生成。

```json
{
  "eval_id": 0,
  "eval_name": "e2e-basic-form-fill",
  "prompt": "用户的 prompt",
  "assertions": []
}
```

---

## timing.json

每次子代理任务完成时，捕获计时数据。

**如何捕获：** 子代理任务完成时，通知中包含 `total_tokens` 和 `duration_ms`。收到通知后立即保存——这些数据不会在其他地方持久化。

```json
{
  "total_tokens": 84852,
  "duration_ms": 23332,
  "total_duration_seconds": 23.3
}
```

---

## grading.json

Grader 的输出。位于 `<run-dir>/grading.json`。

```json
{
  "assertions": [
    {
      "text": "输出包含名称 'John Smith'",
      "passed": true,
      "evidence": "在脚本第3步中发现：'Extracted names: John Smith'"
    }
  ],
  "summary": {
    "passed": 2,
    "failed": 1,
    "total": 3,
    "pass_rate": 0.67
  },
  "execution_metrics": {
    "tool_calls": {
      "Read": 5,
      "Write": 2,
      "Bash": 8
    },
    "total_tool_calls": 15,
    "total_steps": 6,
    "errors_encountered": 0,
    "output_chars": 12450,
    "transcript_chars": 3200
  },
  "timing": {
    "grader_duration_seconds": 26.0
  },
  "claims": [
    {
      "claim": "表单有12个可填写字段",
      "type": "factual",
      "verified": true,
      "evidence": "在 field_info.json 中数到12个字段"
    }
  ],
  "user_notes_summary": {
    "uncertainties": [],
    "needs_review": [],
    "workarounds": []
  },
  "eval_feedback": {
    "suggestions": [],
    "overall": ""
  }
}
```

---

## comparison.json

Blind Comparator 的输出。

```json
{
  "winner": "A",
  "reasoning": "输出A提供了完整解决方案...",
  "rubric": {
    "A": {
      "content": { "correctness": 5, "completeness": 5, "accuracy": 4 },
      "structure": { "organization": 4, "formatting": 5, "usability": 4 },
      "content_score": 4.7,
      "structure_score": 4.3,
      "overall_score": 9.0
    },
    "B": {
      "content": { "correctness": 3, "completeness": 2, "accuracy": 3 },
      "structure": { "organization": 3, "formatting": 2, "usability": 3 },
      "content_score": 2.7,
      "structure_score": 2.7,
      "overall_score": 5.4
    }
  },
  "output_quality": {
    "A": { "score": 9, "strengths": [], "weaknesses": [] },
    "B": { "score": 5, "strengths": [], "weaknesses": [] }
  },
  "assertion_results": {
    "A": { "passed": 4, "total": 5, "pass_rate": 0.80, "details": [] },
    "B": { "passed": 3, "total": 5, "pass_rate": 0.60, "details": [] }
  }
}
```

---

## benchmark.json

人工汇总后生成。

```json
{
  "metadata": {
    "skill_name": "pdf",
    "skill_path": "/path/to/pdf",
    "timestamp": "2026-01-15T10:30:00Z",
    "evals_run": [1, 2, 3],
    "runs_per_configuration": 3
  },
  "runs": [
    {
      "eval_id": 1,
      "eval_name": "basic-form",
      "configuration": "with_skill",
      "run_number": 1,
      "result": {
        "pass_rate": 0.85,
        "passed": 6,
        "failed": 1,
        "total": 7,
        "time_seconds": 42.5,
        "tokens": 3800,
        "tool_calls": 18,
        "errors": 0
      },
      "assertions": [
        {"text": "...", "passed": true, "evidence": "..."}
      ],
      "notes": []
    }
  ],
  "run_summary": {
    "with_skill": {
      "pass_rate": { "mean": 0.85, "stddev": 0.05, "min": 0.80, "max": 0.90 },
      "time_seconds": { "mean": 45.0, "stddev": 12.0, "min": 32.0, "max": 58.0 },
      "tokens": { "mean": 3800, "stddev": 400, "min": 3200, "max": 4100 }
    },
    "without_skill": {
      "pass_rate": { "mean": 0.35, "stddev": 0.08, "min": 0.28, "max": 0.45 },
      "time_seconds": { "mean": 32.0, "stddev": 8.0, "min": 24.0, "max": 42.0 },
      "tokens": { "mean": 2100, "stddev": 300, "min": 1800, "max": 2500 }
    },
    "delta": {
      "pass_rate": "+0.50",
      "time_seconds": "+13.0",
      "tokens": "+1700"
    }
  },
  "notes": [
    "断言 '输出是 PDF 文件' 在两个配置中 100% 通过——可能不区分 skill 价值",
    "Eval 3 显示高方差——可能是 flaky",
    "Without-skill 在表格提取断言上始终失败"
  ]
}
```

**重要：** viewer 读取的确切字段名如上。使用 `configuration` 而不是 `config`，在 `result` 对象下嵌套 `pass_rate` 而不是顶层。如果字段名不对，viewer 会显示空值/零值。生成 benchmark.json 时始终参考此 schema。

---

## 通用字段约定

所有 JSON 文件遵循以下约定：

- `snake_case`: 所有字段名使用蛇形命名法
- `pass_rate`: 浮点数 0.0 到 1.0
- `timestamps`: ISO 8601 格式（如 "2026-01-15T10:30:00Z"）
- `durations`: 秒为单位（浮点数或整数）
- `tokens`: 整数
- `配置名`: `"with_skill"` 或 `"without_skill"`（用于 benchmark viewer 分组和颜色编码）
