data_dict = {}

# Open the file for reading
with open('sub_count.txt', 'r') as file:
    # Iterate through each line in the file
    for line in file:
        # Split the line into a list of integers
        values = [int(val) for val in line.strip().split(',')]
        
        # Check if there are at least 4 values
        if len(values) >= 4:
            second, third, fourth = values[1:4]  # Extract the second, third, and fourth values
            first = values[0]  # Extract the first value
            
            # Create a key if it doesn't exist and append the first value to the list
            key = (second, third, fourth)
            if key not in data_dict:
                data_dict[key] = []
            data_dict[key].append(first)


# Initialize counters
count_1_element = 0
count_same_element = 0
count_diff_element_no_duplicates = 0
count_diff_element_with_duplicates = 0

# Iterate through the dictionary
for key, values in data_dict.items():
    # Check if there is only 1 element in the list
    if len(values) == 1:
        count_1_element += 1
    # Check if all elements in the list are the same
    elif all(value == values[0] for value in values):
        count_same_element += 1
    # Check if there are different elements and no duplicates
    elif len(set(values)) == len(values):
        count_diff_element_no_duplicates += 1
    # Check if there are different elements and duplicates
    else:
        count_diff_element_with_duplicates += 1

# Print the results
print("Number of keys that contain 1 element in their list:", count_1_element*100/len(data_dict))
print("Number of keys that contain the same element in their list:", count_same_element*100/len(data_dict))
print("Number of keys that contain different elements and no duplicates:", count_diff_element_no_duplicates*100/len(data_dict))
print("Number of keys that contain different elements and duplicates:", count_diff_element_with_duplicates*100/len(data_dict))

# Now, data_dict contains the desired data structure
# print(data_dict)