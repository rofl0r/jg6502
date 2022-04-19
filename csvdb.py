class CSVDB():
	def __init__(self, filename, separator=';'):
		with open(filename, 'r') as f:
			h = f.readline()
			self.cols = h.strip().split(separator)
			self.rows = []
			self.separator = separator
			while 1:
				l = f.readline()
				if l == '': break
				l = l.strip()
				if len(l) == 0 or l[0] == '#': continue
				self.rows.append(l.split(separator))
	def get_cols(self):
		return self.cols
	def query(self, selectorfunc, ctx=None):
		results = []
		for fields in self.rows:
			row = {}
			i = 0
			for field in fields:
				row[self.cols[i]] = field
				i += 1
			if selectorfunc(row, ctx): results.append(row)
		return results
