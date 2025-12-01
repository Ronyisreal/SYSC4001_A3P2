#!/bin/bash

# Script to generate test files for TA marking system

echo "Generating rubric file..."
cat > rubric.txt << EOF
1, A
2, B
3, C
4, D
5, E
EOF

echo "Rubric file created."

echo "Generating exam files..."

# Create 20 exam files with student numbers 0001-0020
for i in {1..20}; do
    student_num=$(printf "%04d" $i)
    filename="exam_${student_num}.txt"
    echo "$student_num" > $filename
    echo "Created $filename"
done

# Create the final exam file with student 9999 to signal end
echo "9999" > exam_9999.txt
echo "Created exam_9999.txt (end marker)"

echo ""
echo "Test files generated successfully!"
echo "- 1 rubric file (rubric.txt)"
echo "- 20 exam files (exam_0001.txt to exam_0020.txt)"
echo "- 1 end marker file (exam_9999.txt)"
