import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker

def chartCSVArray(csvArrayInput, xAxis, yAxis, title, output_file = None, isLogX = False):
	plt.figure(figsize=(8, 6))
	colors = ['blue', 'green', 'red', 'orange']  # Colors for each dataset

	for i, item in enumerate(csvArrayInput):
		df = pd.read_csv(item)
		df.columns = df.columns.str.strip()  # Strip whitespace from column names
		color = colors[i % len(colors)]
		clean_label = ' '.join(item.split('_')[:2]).title()
		plt.scatter(df[xAxis], df[yAxis], color=color, marker='o', s=10, label=clean_label)

	if isLogX:
		plt.xscale('log')
		plt.gca().xaxis.set_major_formatter(mticker.FuncFormatter(lambda x, _: f'{int(x):,}'))

	plt.grid(True, alpha=0.3)
	# Add labels, title, and legend
	plt.xlabel(xAxis)
	plt.ylabel(yAxis)
	plt.title(title)
	plt.legend(loc='upper left')
	if output_file:
		plt.savefig(output_file, bbox_inches='tight')  # Save chart to an image
		print(f"Chart saved to {output_file}")
	else:
		plt.show()  # Show chart if no output file is specified

def main():

	#how much overhead we're causing
	overheadArrays = [
		"BEST_FIT_OVERHEAD.csv",
		"BUDDY_FIT_OVERHEAD.csv",
		"WORST_FIT_OVERHEAD.csv",
		"FIRST_FIT_OVERHEAD.csv"
	]
	chartCSVArray(overheadArrays, "Iteration", "Overhead Bytes", "Overhead during an average program run", "overhead.png")
	
	#for malloc
	timeArrays = [
		"BEST_FIT_TIME.csv",
		"BUDDY_FIT_TIME.csv",
		"WORST_FIT_TIME.csv",
		"FIRST_FIT_TIME.csv"
	]
	chartCSVArray(timeArrays, "Bytes Allocated", "Malloc Time Taken", "Malloc Time (ns) As a Function of Bytes Allocated", "malloc.png", True)

	#for free
	timeArrays = [
		"BEST_FIT_TIME.csv",
		"BUDDY_FIT_TIME.csv",
		"WORST_FIT_TIME.csv",
		"FIRST_FIT_TIME.csv"
	]
	chartCSVArray(timeArrays, "Bytes Allocated", "Free Time Taken", "Free Time (ns) As a Function of Bytes Allocated", "free.png", True)

	#Percentage of memory actually utilized over time (as we increase mallocs and frees over time)
	utilizationArrays = [
		"BEST_FIT_UTILIZATION.csv",
		"BUDDY_FIT_UTILIZATION.csv",
		"WORST_FIT_UTILIZATION.csv",
		"FIRST_FIT_UTILIZATION.csv"
	]
	chartCSVArray(utilizationArrays, "Iteration", "Average Percent Utilization", "Average percent utilization during an average program run", "util.png")

	# Bar chart of final average utilization per strategy
	df = pd.read_csv("UTILIZATION_SUMMARY.csv")
	df.columns = df.columns.str.strip()
	df["Strategy"] = df["Strategy"].str.replace("_FIT", "", regex=False).str.title()
	plt.figure(figsize=(8, 6))
	plt.bar(df["Strategy"], df["Average Percent Utilization"], color=['blue', 'green', 'red', 'orange', 'purple'])
	plt.xlabel("Strategy")
	plt.ylabel("Average Percent Utilization (%)")
	plt.title("Final Average Memory Utilization by Strategy")
	plt.grid(True, alpha=0.3, axis='y')
	plt.savefig("utilization_bar.png", bbox_inches='tight')
	print("Chart saved to utilization_bar.png")
		

if __name__ == "__main__":
	main()